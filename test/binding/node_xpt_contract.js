'use strict';

// XPT transport contract for the WSE Node-API binding.
//
// Hardware-free: both UDP endpoints are loopback sockets inside this process, the serial case opens
// a device that cannot exist, the transport-failure HTTP and TCP cases target a closed local port,
// and the HTTP status case is answered by a loopback server this process starts on a worker thread.
// No external network, serial device, or server is required.

const assert = require('node:assert/strict');
const path = require('node:path');
const { Worker } = require('node:worker_threads');

// The classes under test live in the package entry point, so the addon is injected and the
// package is required exactly as an application would.
process.env.WSE_NODE_ADDON = path.resolve(process.argv[2]);
const binding = require(path.resolve(process.argv[3]));

function verifyUdpLoopback() {
  const context = { timeoutMs: 1000 };

  const receiver = new binding.UdpClient();
  const sender = new binding.UdpClient();
  try {
    receiver.bind('127.0.0.1', 0, context);
    sender.bind('127.0.0.1', 0, context);
    assert.equal(receiver.isOpen(), true);

    const local = receiver.localEndpoint();
    assert.notEqual(local.port, 0, 'binding to port zero must report the assigned port');

    const payload = Buffer.from([1, 2, 3, 4, 5]);
    assert.equal(sender.sendTo(local.host, local.port, payload, context), payload.length);

    const datagram = receiver.receiveFrom(binding.UdpClient.maximumDatagramSize(), context);
    assert.deepEqual([...datagram.payload], [...payload]);
    assert.equal(datagram.source.port, sender.localEndpoint().port);

    sender.sendTo(local.host, local.port, Buffer.from([0, 127, 255, 1, 2]), context);
    let saved;
    assert.throws(() => receiver.receiveFrom(3, context), (error) => {
      assert.ok(error instanceof Error);
      assert.equal(error.code, binding.TransportErrorCode.DatagramTruncated);
      assert.equal(error.bytesTransferred, 3);
      assert.deepEqual([...error.receivedData], [0, 127, 255]);
      assert.deepEqual(error.sourceEndpoint, sender.localEndpoint());
      saved = error;
      return true;
    });
    sender.sendTo(local.host, local.port, Buffer.from('next'), context);
    assert.equal(receiver.receiveFrom(10, context).payload.toString(), 'next');
    assert.deepEqual([...saved.receivedData], [0, 127, 255]);

    // XPT has no default timeout, so an idle receive must fail once its deadline elapses.
    assert.throws(
      () => receiver.receiveFrom(binding.UdpClient.maximumDatagramSize(), { timeoutMs: 50 }),
      (error) => {
        assert.equal(error.code, binding.TransportErrorCode.TimedOut);
        assert.equal(error.category, 5, 'TimedOut must normalize to the Timeout category');
        assert.equal(error.bytesTransferred, undefined);
        return true;
      },
    );

    // A cancelled context must be observed cooperatively rather than ignored.
    const cancellation = new binding.CancellationSource();
    cancellation.cancel();
    assert.equal(cancellation.isCancellationRequested(), true);
    assert.throws(
      () => receiver.receiveFrom(65507, { timeoutMs: 5000, cancellation }),
      (error) => {
        assert.equal(error.code, binding.TransportErrorCode.Cancelled);
        return true;
      },
    );

    receiver.close();
    assert.deepEqual([...saved.receivedData], [0, 127, 255]);
    assert.equal(receiver.isOpen(), false);
    receiver.close();
  } finally {
    sender.close();
  }
}

function verifySendProgress() {
  const tcp = new binding.TcpClient(), serial = new binding.SerialPort(), udp = new binding.UdpClient();
  const context = {timeoutMs: 100};
  try {
    for (const send of [() => tcp.send(Buffer.from('x'), context),
      () => serial.send(Buffer.from('x'), context),
      () => udp.sendTo('127.0.0.1', 0, Buffer.from('x'), context)]) {
      assert.throws(send, error => {
        assert.ok(error instanceof Error);
        assert.equal(error.bytesTransferred, 0);
        assert.equal(error.receivedData, undefined);
        assert.equal(error.sourceEndpoint, undefined);
        assert.notEqual(error.category, 0);
        assert.notEqual(error.code, 0);
        return true;
      });
    }
  } finally { tcp.disconnect(); serial.close(); udp.close(); }
}

function verifyArgumentContracts() {
  const client = new binding.UdpClient();
  assert.throws(() => client.bind('127.0.0.1', 0, { timeoutMs: -1 }), { name: 'RangeError' });
  assert.throws(() => client.bind('127.0.0.1', 0, {}), { name: 'TypeError' });
  client.close();
}

function verifySerial() {
  const context = { timeoutMs: 200 };
  const port = new binding.SerialPort();
  try {
    assert.equal(port.isOpen(), false);
    assert.throws(
      () => port.open('WSE_NONEXISTENT_PORT', 9600, context),
      (error) => {
        assert.notEqual(error.category, 0);
        return true;
      },
    );
    assert.equal(port.isOpen(), false, 'a failed open must leave the port closed');
  } finally {
    port.close();
  }
}

function verifyTcp() {
  const context = { timeoutMs: 200 };
  const client = new binding.TcpClient();
  try {
    assert.equal(client.isConnected(), false);
    // Port 1 on loopback is not listening, so the attempt must fail structurally.
    assert.throws(
      () => client.connect('127.0.0.1', 1, context),
      (error) => {
        assert.notEqual(error.category, 0);
        return true;
      },
    );
    assert.equal(client.isConnected(), false);
  } finally {
    client.disconnect();
  }
}

function verifyHttp() {
  const context = { timeoutMs: 200 };
  assert.throws(
    () => binding.httpExecute(
      {
        method: binding.HttpMethod.Post,
        url: 'http://127.0.0.1:1/wse',
        headers: [{ name: 'Content-Type', value: 'application/octet-stream' }],
        body: Buffer.from([0, 1, 2]),
      },
      4096,
      context,
    ),
    (error) => {
      assert.notEqual(error.category, 0);
      // A transport failure has no response to report, so the property stays absent.
      assert.equal(error.response, undefined);
      return true;
    },
  );
}

// An XPT call blocks the calling thread, so the answering server has to live on another thread;
// a worker keeps it inside this process and off the loop that is about to block.
const STATUS_SERVER = `
const http = require('node:http');
const { parentPort } = require('node:worker_threads');
const server = http.createServer((request, response) => {
  request.resume();
  request.on('end', () => {
    response.writeHead(404, { 'Content-Type': 'text/plain', 'X-Wse-Contract': 'status' });
    response.end('missing');
  });
});
server.listen(0, '127.0.0.1', () => parentPort.postMessage(server.address().port));
`;

// A 4xx answer is a failure that still carries a response. The call keeps throwing, and the
// received response rides on the thrown error so the status code is not lost.
async function verifyHttpStatusError() {
  const worker = new Worker(STATUS_SERVER, { eval: true });
  try {
    const port = await new Promise((resolve, reject) => {
      worker.once('message', resolve);
      worker.once('error', reject);
    });
    assert.throws(
      () => binding.httpExecute(
        { method: binding.HttpMethod.Get, url: `http://127.0.0.1:${port}/missing` },
        4096,
        { timeoutMs: 5000 },
      ),
      (error) => {
        assert.equal(error.code, binding.TransportErrorCode.HttpStatusError);
        assert.ok(error.response, 'an HTTP status failure must carry its response');
        assert.equal(error.response.statusCode, 404);
        assert.equal(error.response.attemptCount, 1);
        assert.ok(error.response.headers.some(
          (header) => header.name.toLowerCase() === 'x-wse-contract'));
        assert.ok(Buffer.isBuffer(error.response.body));
        assert.equal(error.response.body.toString('utf8'), 'missing');
        return true;
      },
    );
  } finally {
    await worker.terminate();
  }
}

async function main() {
  assert.equal(binding.runtimeInfo().components.xpt, true);
  verifyUdpLoopback();
  verifySendProgress();
  verifyArgumentContracts();
  verifySerial();
  verifyTcp();
  verifyHttp();
  await verifyHttpStatusError();
  console.log('node XPT contract passed');
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
