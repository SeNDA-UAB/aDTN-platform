package cat.uab.senda.adtn.comm;

/**
 * Exception to control bad operation codes from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class OpNotSuportedException extends Exception {

	private static final long serialVersionUID = -8064290241543916006L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public OpNotSuportedException(String message) {
		super(message);
	}
}
