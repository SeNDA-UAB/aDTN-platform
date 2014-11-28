package src.cat.uab.senda.adtn.comm;

/**
 * Exception to control code bind errors from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class CodeBindException extends Exception {

	private static final long serialVersionUID = 8828767360916020884L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public CodeBindException(String message) {
		super(message);
	}
}
