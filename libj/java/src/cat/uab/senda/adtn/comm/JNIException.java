package src.cat.uab.senda.adtn.comm;

/**
 * Exception to control any JNI errors calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class JNIException extends Exception {

	private static final long serialVersionUID = 4690255849912697638L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public JNIException(String message) {
		super(message);
	}
}
