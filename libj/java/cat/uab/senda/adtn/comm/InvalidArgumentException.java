package cat.uab.senda.adtn.comm;

/**
 * Exception to control invalid argument errors from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class InvalidArgumentException extends Exception {

	private static final long serialVersionUID = -5645004448250014349L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public InvalidArgumentException(String message) {
		super(message);
	}
}
