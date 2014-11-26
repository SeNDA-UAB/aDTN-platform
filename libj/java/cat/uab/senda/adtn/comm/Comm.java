package cat.uab.senda.adtn.comm;

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.text.ParseException;



/**
* This class is a java interface for the <a href="https://github.com/SeNDA-UAB/aDTN-platform">active DTN platform</a>
*
* This class describes the methods for create, bind, close, send and receive a bundle over the aDTN platform.
*
* @author Senda DevLab {@literal <developers@senda.uab.cat>}
* @version 0.3
*
*/
public class Comm {
	// Suppresses default constructor, ensuring non-instantiability.
	private Comm() {
	};

	static {
		System.loadLibrary("NativeAdtnApi");
	}

	// Option codes

	/**
	 * Option code to set or get the primary block flags of the new bundles.
	 */
	public static final int OP_PROC_FLAGS = 1;
	
	/**
	 * Option code to set or get the lifetime of the new generate bundles.
	 */
	public static final int OP_LIFETIME = 2;
	
	/**
	 * Option code to set or get the extension block flags of the new bundles.
	 */
	public static final int OP_BLOCK_FLAGS = 3;
	
	/**
	 * Option code to set or get the destination field of the new bundles.
	 */
	public static final int OP_DEST = 4;
	
	/**
	 * Option code to set or get the source field of the new bundles.
	 */
	public static final int OP_SOURCE = 5;
	
	/**
	 * Option code to set or get the report field of the new bundles.
	 */
	public static final int OP_REPORT = 6;
	
	/**
	 * Option code to set or get the custom field of the new bundles.
	 */
	public static final int OP_CUSTOM = 7;
	
	/**
	 * Option code to get the primary block flags of the new bundles.
	 */
	public static final int OP_LAST_TIMESTAMP = 8;

	// Code options

	/**
	 * Code to set the Routing code.
	 */
	public static final int ROUTING_CODE = 0x01;
	
	/**
	 * Code to set the Priority code.
	 */
	public static final int PRIO_CODE = 0x02;
	
	/**
	 * Code to set the Life control code.
	 */
	public static final int LIFE_CODE = 0x03;

	// Extension block flags

	/**
	 * The Block must be replicated in each fragment.
	 */
	public static final int B_REP_FR = 0x01;
	
	/**
	 * Transmit status error if Block can't be processed.
	 */
	public static final int B_ERR_NP = 0x02;
	
	/**
	 * Delete Bundle if Block can't be processed.
	 */
	public static final int B_DEL_NP = 0x04;
	
	/**
	 * Set as the last Block.
	 */
	public static final int B_LAST   = 0x08;
	
	/**
	 * Discard Block if it can't be processed.
	 */
	public static final int B_DIS_NP = 0x10;
	
	/**
	 * Block was forwarded without being processed.
	 */
	public static final int B_WFW_NP = 0x20;
	
	/**
	 * Block contains EID-reference field.
	 */
	public static final int B_EID_RE = 0x40;                   

	// Primary block flags

	/**
	 * Bundle is a fragment.
	 */
	public static final int H_FRAG = 0x01;
	
	/**
	 * Administrative record.
	 */
	public static final int H_ADMR = 0x02;
	
	/**
	 * Bundle must not be fragmented.
	 */
	public static final int H_NOTF = 0x04;
	
	/**
	 * Custody transfer requested.
	 */
	public static final int H_CSTR = 0x08;
	
	/**
	 * Destination is singleton.
	 */
	public static final int H_DESS = 0x10;
	
	/**
	 * Acknowledgment is requested.
	 */
	public static final int H_ACKR = 0x20;
	
	/**
	 * Set Bundle's priority to Bulk.
	 */
	public static final int H_COS_BULK = 0x80;
	
	/**
	 * Set Bundle's priority to Normal.
	 */
	public static final int H_COS_NORM = 0x100;
	
	/**
	 * Set Bundle's priority to Expedited.
	 */
	public static final int H_COS_EXP  = 0x200;
	
	/**
	 * Request reporting of Bundle reception.
	 */
	public static final int H_SR_BREC = 0x4000;
	
	/**
	 * Request reporting of custody acceptance.
	 */
	public static final int H_SR_CACC = 0x8000;
	
	/**
	 * Request reporting of Bundle forwarding.
	 */
	public static final int H_SR_BFRW = 0x10000;
	
	/**
	 * Request reporting of Bundle delivery.
	 */
	public static final int H_SR_BDEL = 0x20000;
	
	/**
	 * Request reporting of Bundle deletion.
	 */
	public static final int H_SR_BDLT = 0x40000;

	/**
	 * Creates a socket over the aDTN platform.
	 * This method will return a socket identifier, this socket will be created with the default configuration parameters
	 * (this parameters are in the path of the aDTN platform installation).
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * 
	 * @return the socket identifier.
	 * @throws SocketException If cannot allocate the new socket.
	 * @throws FileNotFoundException The global configuration is missing in the file or the configuration file cannot be found.
	 * @throws ParseException The data field is missing in the configuration file.
	 */
	public static native int adtnSocket() throws SocketException, FileNotFoundException, ParseException;
	
	/**
	 * Creates a socket over the aDTN platform.
	 * This method will return a socket identifier, this socket will be created with the parameters in the file pointed by
	 * {@code dataPath}.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param  dataPath path to the parameter file.
	 * @return the socket identifier.
	 * @throws SocketException If cannot allocate the new socket.
	 * @throws FileNotFoundException The global configuration is missing in the file or the configuration file cannot be found.
	 * @throws ParseException The data field is missing in the configuration file.
	 */
	public static native int adtnSocket(String dataPath) throws SocketException, FileNotFoundException, ParseException;
	
	/**
	 * Binds a socket to an address.
	 * This method binds a socket identifier with an address defined by {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 	the socket identifier.
	 * @param addr 	the information to bind.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws FileNotFoundException Cannot find the Bundle Agent, check if it's running.
	 * @throws IllegalAccessException The application hasn't enough permissions to bind the socket.
	 * @throws AddressInUseException The address is already in use.
	 */
	public static native void adtnBind(int s, SockAddrT addr) throws SocketException, FileNotFoundException, IllegalAccessException, AddressInUseException;
	
	/**
	 * Close the socket.
	 * This method closes the socket {@code s} freeing all the memory associated to the socket, the structures created at calling
	 * adtnBind are deleted to.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 	the socket identifier.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws IllegalAccessException The application hasn't enough permissions to eliminate the socket.
	 */
	public static native void adtnClose(int s) throws SocketException, IllegalAccessException;
	
	/**
	 * Close the socket, also deletes waiting data.
	 * This method is similar to {@link #adtnClose(int s)}, adding the deletion of waiting bundles in the queue.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 	the socket identifier.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws FileNotFoundException Cannot find the aDTN service, check if it's running.
	 * @throws IllegalAccessException The application hasn't enough permissions to eliminate the socket.
	 */
	public static native void adtnShutdown(int s) throws SocketException, FileNotFoundException, IllegalAccessException;
	
	/**
	 * Sets the codes to execute.
	 * This method lets to set the code for the {@code codeOption}, the code has to be write in {@code C}.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param codeOption 	the code option wanted to set ({@link #ROUTING_CODE}, {@link #PRIO_CODE}, {@link #LIFE_CODE}).
	 * @param code 			the string containing the source in {@code C} to set.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws InvalidArgumentException Invalid code, check if {@code code} it's a valid {@code C} code.
	 * @throws CodeBindException A code is already binded to this option.
	 */
	public static native void adtnSetCodeOption(int s, int codeOption, String code) throws SocketException, InvalidArgumentException, CodeBindException;
	
	/**
	 * Sets the codes to execute.
	 * This method is similar to {@link #adtnSetCodeOption(int s, int codeOption, String code)}, letting the use of a file or the code in the string.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param codeOption    the code option wanted to set ({@link #ROUTING_CODE}, {@link #PRIO_CODE}, {@link #LIFE_CODE}).
	 * @param code 			the string containing the source in {@code C} to set or the path to the file with the source in {@code C}.
	 * @param fromFile 		select if the code is in the file {@code 1} or in the string {@code 0}.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws InvalidArgumentException Invalid code, check if {@code code} it's a valid {@code C} code or a valid file.
	 * @throws CodeBindException A code is already binded to this option.
	 */
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile) throws SocketException, InvalidArgumentException, CodeBindException;
	
	/**
	 * Sets the codes to execute.
	 * This method is similar to {@link #adtnSetCodeOption(int s, int codeOption, String code)}, letting the use of a file or the code in the string and the option of replace old codes.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param codeOption    the code option wanted to set ({@link #ROUTING_CODE}, {@link #PRIO_CODE}, {@link #LIFE_CODE}).
	 * @param code 			the string containing the source in {@code C} to set or the path to the file with the source in {@code C}.
	 * @param fromFile 		select if the code is in the file {@code 1} or in the string {@code 0}.
	 * @param replace 		if value is {@code 1} replace any existing code, else if value is {@code 0} and an exists a code the function fails.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws InvalidArgumentException Invalid code, check if {@code code} it's a valid {@code C} code or a valid file.
	 * @throws CodeBindException A code is already binded to this option.
	 */
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile, int replace) throws SocketException, InvalidArgumentException, CodeBindException;
	
	/**
	 * Remove the codes associated.
	 * This method removes the code {@code codeOption} setting again the default code.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param codeOption 	the code option wanted to set ({@link #ROUTING_CODE}, {@link #PRIO_CODE}, {@link #LIFE_CODE}).
	 * @throws SocketException The socket identifier is not valid.
	 * @throws InvalidArgumentException The {@code codeOption} is not valid.
	 */
	public static native void adtnRemoveCodeOption(int s, int codeOption) throws SocketException, InvalidArgumentException;
	
	/**
	 * Sets an option to the new bundles generated.
	 * This method allows to set options to the new bundles generated.
	 * <p>
	 * This options are:
	 *
	 * <ul>
	 * <li>
	 * {@link #OP_PROC_FLAGS}: sets the primary block flags, the {@code value} must be an {@link Integer} combination of the primary block tags:
	 * <ul>
	 * <li> {@link #H_FRAG} </li>
	 * <li> {@link #H_ADMR} </li>
	 * <li> {@link #H_NOTF} </li>
	 * <li> {@link #H_CSTR} </li>
	 * <li> {@link #H_DESS} </li>
	 * <li> {@link #H_ACKR} </li>
	 * <li> {@link #H_COS_BULK} </li>
	 * <li> {@link #H_COS_NORM} </li>
	 * <li> {@link #H_COS_EXP} </li>
	 * <li> {@link #H_SR_BREC} </li>
	 * <li> {@link #H_SR_CACC} </li>
	 * <li> {@link #H_SR_BFRW} </li>
	 * <li> {@link #H_SR_BDEL} </li>
	 * <li> {@link #H_SR_BDLT} </li>
	 * </ul>
	 * </li>
	 * <li>
	 * {@link #OP_LIFETIME}: an {@link Long} value that hold the lifetime for the new bundles.
	 * </li>
	 * <li>
	 * {@link #OP_BLOCK_FLAGS}: sets the extension block flags, the {@code value} must be an {@link Integer} combination of the extension block tags:
	 * <ul>
	 * <li> {@link #B_REP_FR} </li>
	 * <li> {@link #B_ERR_NP} </li>
	 * <li> {@link #B_DEL_NP} </li>
	 * <li> {@link #B_LAST} </li>
	 * <li> {@link #B_DIS_NP} </li>
	 * <li> {@link #B_WFW_NP} </li>
	 * <li> {@link #B_EID_RE} </li>
	 * </ul>
	 * </li>
	 * <li>
	 * {@link #OP_DEST}: a {@link cat.uab.senda.adtn.comm.SockAddrT} with the destination for the bundle.
	 * </li>
	 * <li>
	 * {@link #OP_SOURCE}: a {@link cat.uab.senda.adtn.comm.SockAddrT} with the source for the bundle.
	 * </li>
	 * <li>
	 * {@link #OP_REPORT}: a {@link cat.uab.senda.adtn.comm.SockAddrT} with the report for the bundle.
	 * </li>
	 * <li>
	 * {@link #OP_CUSTOM}: a {@link cat.uab.senda.adtn.comm.SockAddrT} with the custom for the bundle.
	 * </li>
	 * </ul>
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param optionCode    the option code, as seen over.
	 * @param value 		the value to set.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws OpNotSuportedException The {@code optionCode} is not a valid option.
	 */
	public static native void adtnSetSocketOption(int s, int optionCode, Object value) throws SocketException, OpNotSuportedException;
	
	/**
	 * Gets the value of an option.
	 * This method allows to get the value of the options to the new bundles generated.
	 * <p>
	 * This options are:
	 *
	 * <ul>
	 * <li>
	 * {@link #OP_PROC_FLAGS}: gets the primary block flags, the function returns an {@link Integer}.
	 * <li>
	 * {@link #OP_LIFETIME}: gets the lifetime, the function returns an {@link Integer}.
	 * </li>
	 * <li>
	 * {@link #OP_BLOCK_FLAGS}: gets the extension block flags, the function returns an {@link Integer}.
	 * </li>
	 * <li>
	 * {@link #OP_DEST}: gets the destination, the function returns a {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * </li>
	 * <li>
	 * {@link #OP_SOURCE}: gets the source, the function returns a {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * </li>
	 * <li>
	 * {@link #OP_REPORT}: gets the report, the function returns a {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * </li>
	 * <li>
	 * {@link #OP_CUSTOM}: gets the custom, the function returns a {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * </li>
	 * <li>
	 * {@link #OP_LAST_TIMESTAMP}: gets the timestamp of the last sended bundle, the function returns a {@link Long}
	 * </li>
	 * </ul>
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param s 			the socket identifier.
	 * @param optionCode    the option code, as seen over.
	 * @return the value of the option.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws OpNotSuportedException The {@code optionCode} is not a valid option.
	 */
	public static native Object adtnGetSocketOption(int s, int optionCode) throws SocketException, OpNotSuportedException;
	
	/**
	 * Sends a bundle to the {@code addr} with the information in {@code data}.
	 * This method allows to generate a bundle with the information in {@code data} and send it to the source {@code addr}.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param  s 		the socket identifier.
	 * @param  addr 	the source {@link cat.uab.senda.adtn.comm.SockAddrT}.
	 * @param  data 	the information to send.
	 * @return a number bigger than {@code 0} if the bundle have been send.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws InvalidArgumentException Invalid address or empty data.
	 * @throws FileNotFoundException Cannot find the aDTN service, check if it's running.
	 * @throws MessageSizeException The data array is too big. The max size of a bundle is 6291456 bytes.
	 */
	public static native int adtnSendTo(int s, SockAddrT addr, byte[] data) throws InvalidArgumentException, SocketException, FileNotFoundException, MessageSizeException;
	
	/**
	 * Recive a bundle and get it's data.
	 * This method allows to put the application to listening. If no message has been received this method will block until some message arrives.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param  s 			the socket identifier.
	 * @param  data 		the array of data to fill.
	 * @param  data_len 	the number of bytes that will be written in {@code data}.
	 * @return the number of bytes received or {@code -1} if fails.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws FileNotFoundException Cannot find the aDTN service, check if it's running.
	 * @throws InvalidArgumentException Invalid data_len.
	 */
	public static native int adtnRecv(int s, byte[] data, int data_len) throws InvalidArgumentException, SocketException, FileNotFoundException;
	
	/**
	 * Receive a bundle and get it's data.
	 * This method is similar to {@link #adtnRecv(int s, byte[] data, int data_len)}, adding the extraction of the sender information.
	 * <p>
	 * If the platform is not active or an error occurred in the creation an exception is generated.
	 * @param  s 			the socket identifier.
	 * @param  data 		the array of data to fill.
	 * @param  data_len 	the number of bytes that will be written in {@code data}.
	 * @param  addr 		the {@link cat.uab.senda.adtn.comm.SockAddrT} to fill with the source information.
	 * @return the number of bytes received or {@code -1} if fails.
	 * @throws SocketException The socket identifier is not valid.
	 * @throws FileNotFoundException Cannot find the aDTN service, check if it's running.
	 * @throws InvalidArgumentException Invalid data_len.
	 */
	public static native int adtnRecvFrom(int s, byte[] data, int data_len, SockAddrT addr) throws InvalidArgumentException, SocketException, FileNotFoundException;

}

