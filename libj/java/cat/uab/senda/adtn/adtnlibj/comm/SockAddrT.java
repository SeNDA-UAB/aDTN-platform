package cat.uab.senda.adtn.adtnlibj.comm;

/**
* This class represents a socket address over the aDTN.
*
*
* @author Senda DevLab {@literal <developers@senda.uab.cat>}
* @version 0.1
*
*/
public class SockAddrT {
	/**
	 * Port of the application.
	 */
	int port;
	/**
	 * Identifier of the platform.
	 */
	String id;
	/**
	 * Create a new socket address.
	 *
	 * @param  id  	identifier of the platform. 
	 * @param  port port of the application.
	 */
	public SockAddrT(String id, int port){
		this.port = port;
		this.id = id;
	}
}