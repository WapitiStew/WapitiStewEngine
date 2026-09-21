import io.wapitistew.wse.RuntimeInfo;
import io.wapitistew.wse.WseRuntime;

/**
 * Hardware-free WSE Java quick start.
 *
 * <p>This is the smallest complete use of the binding: load the native runtime, read what it is,
 * touch no device. It is also the one Java sample the build itself compiles and runs, so it doubles
 * as the gate that proves the binding loads and the JNI call path works.
 *
 * <p>Loading: the binding's static initializer loads the file named by the
 * {@code -Dwse.runtime.path=...} system property by absolute path, then loads {@code wse_jni}
 * through {@code java.library.path}. An absolute load on Windows does not add the containing
 * directory to the dependency search path, so the engine directory must also be on {@code PATH}
 * whenever an enabled component brings its own runtime library beside {@code WonderStewEngine}.
 * Run it as
 * {@code java -Dwse.runtime.path=<dir>\WonderStewEngine.dll -cp <binding-classes>;<output>
 * QuickStart}.
 *
 * <p>Nothing here owns a native handle, so nothing has to be closed: {@code WseRuntime.info()} is
 * static and needs no {@code WseRuntime} instance. Every other sample starts from the record it
 * returns, asking whether the component it needs was built in.
 */
public final class QuickStart {
    private QuickStart() {}

    public static void main(String[] arguments) {
        RuntimeInfo info = WseRuntime.info();
        // A version that is not three dotted numbers means the library that answered is not a WSE
        // runtime this binding understands, so this sample fails loudly rather than going on to
        // build anything on top of it. This is the one place a sample throws on purpose.
        if (!info.version().matches("\\d+\\.\\d+\\.\\d+"))
            throw new IllegalStateException("WSE returned an invalid semantic version");
        // The record prints the runtime version, the binding ABI version, and one flag per optional
        // component; those flags are what the component samples branch on before they call in.
        System.out.println(info);
    }
}
