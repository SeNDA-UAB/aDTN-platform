package src.cat.uab.senda.adtn.comm;

/**
 * Exception to control to big message errors from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class MessageSizeException extends Exception {

	private static final long serialVersionUID = 5154560215879270962L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public MessageSizeException(String message) {
		super(message);
	}
}
