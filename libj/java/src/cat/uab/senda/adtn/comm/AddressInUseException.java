package cat.uab.senda.adtn.comm;

/**
 * Exception to control an address in use error from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class AddressInUseException extends Exception {

	private static final long serialVersionUID = -1429650974748551261L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public AddressInUseException(String message) {
		super(message);
	}
}
