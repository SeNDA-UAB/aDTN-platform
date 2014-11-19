package cat.uab.senda.adtn.adtnlibj.comm;

/**
* @author SENDA
* @version 1.0.1
*
*/
public class PlatfComm{
	
	private PlatfComm(){
		
	};

	static {
		System.loadLibrary("NativeAdtnApi");
	}	
	/**
	* Function to create a socket.
	*
	* This function creates a socket taking the default config file.
	*
	* @return Socket descriptor.
	*/
	public static native int adtnSocket();
	public static native int adtnSocket(String dataPath);
	public static native void adtnBind(int s, SockAddrT addr);
	public static native void adtnClose(int s);
	public static native void adtnShutdown(int s);
	public static native void adtnSetCodeOption(int s, int codeOption, String code);
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile);
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile, int replace);
	public static native void adtnRemoveCodeOption(int s, int codeOption);
	
	public static native void adtnSetSocketOption(int s, int optionCode, Object value);
	public static native Object adtnGetSocketOption(int s, int optionCode);

	public static native int adtnSendTo(int s, SockAddrT addr, byte[] data);
	public static native int adtnRecv(int s, byte[] data, int data_len);
	public static native int adtnRecvFrom(int s, byte[] data, int data_len, SockAddrT addr);

	/**
	*
	*/
	public static final int OP_PROC_FLAGS = 1;
	public static final int OP_LIFETIME = 2;
	public static final int OP_BLOCK_FLAGS = 3;
	public static final int OP_DEST = 4;
	public static final int OP_SOURCE = 5;
	public static final int OP_REPORT = 6;
	public static final int OP_CUSTOM = 7;
	public static final int OP_LAST_TIMESTAMP = 8;

	public static final int B_REP_FR = 0x01;                    // Must be replicated in each fragment
	public static final int B_ERR_NP = 0x02;                    // Transmit status error if block can't be processed
	public static final int B_DEL_NP = 0x04;                    // Delete Bundle if block can't be processed
	public static final int B_LAST   = 0x08;                    // Is the last block
	public static final int B_DIS_NP = 0x10;                    // Discard block if it can't be processed
	public static final int B_WFW_NP = 0x20;                    // Block was forwarded without being processed
	public static final int B_EID_RE = 0x40;					// Block contains EID-reference field

	public static final int H_FRAG = 0x01;                      // Bundle is a fragment
	public static final int H_ADMR = 0x02;                      // Administrative record
	public static final int H_NOTF = 0x04;                      // Must not be fragmented
	public static final int H_CSTR = 0x08;                      // Custody transfer requested
	public static final int H_DESS = 0x10;                      // Destination is singleton
	public static final int H_ACKR = 0x20;                      // Acknowledgment is requested

	public static final int H_COS_BULK = 0x80;                  // bundle’s priority == bulk
	public static final int H_COS_NORM = 0x100;                 // bundle’s priority == normal
	public static final int H_COS_EXP  = 0x200;                 // bundle’s priority == expedited

	public static final int H_SR_BREC = 0x4000;                 // Request reporting of bundle reception
	public static final int H_SR_CACC = 0x8000;                 // Request reporting of custody acceptance
	public static final int H_SR_BFRW = 0x10000;                // Request reporting of bundle forwarding
	public static final int H_SR_BDEL = 0x20000;                // Request reporting of bundle delivery
	public static final int H_SR_BDLT = 0x40000;                // Request reporting of bundle deletion

	
	
}
