import pathlib
import re
import unittest


WSE_ROOT = pathlib.Path(__file__).resolve().parents[2]
INVENTORY = WSE_ROOT / "tools" / "pi" / "inspect_pi4_environment.sh"
SMOKE = WSE_ROOT / "tools" / "pi" / "run_pi4_smoke.sh"
README_EN = WSE_ROOT / "tools" / "pi" / "README.md"
README_JA = WSE_ROOT / "tools" / "pi" / "README.ja.md"


class PiToolContract(unittest.TestCase):
    def test_inventory_is_strict_and_privacy_filtered(self):
        text = INVENTORY.read_text(encoding="utf-8")
        self.assertIn("set -eu", text)
        self.assertIn("aarch64|arm64", text)
        for key in (
            "wse.pi.inventory_schema",
            "platform.pi_generation",
            "display.vulkan_api_version",
            "network.default_route",
            "network.ssh_listener",
            "health.throttled",
            "privacy.hostname omitted",
            "privacy.network_addresses omitted",
            "privacy.account_name omitted",
            "privacy.device_identifiers omitted",
        ):
            self.assertIn(key, text)

        for forbidden in (
            "/etc/machine-id",
            "/proc/cpuinfo",
            "ip address",
            "ip addr",
            "ifconfig",
            "hostnamectl",
            "printenv",
        ):
            self.assertNotIn(forbidden, text)
        self.assertIsNone(re.search(r"(?<![0-9])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![0-9])", text))

    def test_smoke_keeps_arm64_and_contract_boundary(self):
        text = SMOKE.read_text(encoding="utf-8")
        self.assertIn("set -eu", text)
        self.assertIn("aarch64|arm64", text)
        for executable in (
            "wse.core.characterization",
            "wse.core.runtime_contract",
            "wse.xpt.tcp_loopback",
            "wse.xpt.udp_loopback",
            "wse.xpt.retry_policy_contract",
            "wse.xpt.http_contract",
            "wse.xpt.serial_port_contract",
        ):
            self.assertIn(executable, text)

    def test_tool_documentation_is_paired(self):
        english = README_EN.read_text(encoding="utf-8")
        japanese = README_JA.read_text(encoding="utf-8")
        self.assertIn("> Canonical language: English", english)
        self.assertIn("> Canonical source: [", japanese)
        manifest = (WSE_ROOT / "doc/DocumentationManifest.tsv").read_text(encoding="utf-8")
        row = next(line for line in manifest.splitlines() if line.startswith("pi-tools|"))
        synchronized = row.split("|")[4]
        for text in (english, japanese):
            self.assertIn("> Documentation version: WSE 1.0.0", text)
            self.assertIn("> Last synchronized: " + synchronized, text)
            self.assertIn("inspect_pi4_environment.sh", text)
            self.assertIn("run_pi4_smoke.sh", text)


if __name__ == "__main__":
    unittest.main()
