package cat.uab.senda.adtn.adtnlibj.comm;

public class PlatfComm{
	
	private PlatfComm(){
		
	};

	static {
		System.loadLibrary("NativeAdtnApi");
	}	
	public static native int adtnSocket();
	public static native int adtnSocket(String dataPath);
	public static native void adtnBind(int s, SockAddrT addr);
	public static native void adtnClose(int s);
	public static native void adtnShutdown(int s);
	public static native void adtnSetCodeOption(int s, int codeOption, String code);
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile);
	public static native void adtnSetCodeOption(int s, int codeOption, String code, int fromFile, int replace);
	public static native void adtnRemoveCodeOption(int s, int codeOption);
	
	public static native void adtnSetSocketOption(int s, int optionCode, int value);
	public static native void adtnSetSocketOption(int s, int optionCode, long value);
	public static native void adtnSetSocketOption(int s, int optionCode, String value);

	public static native int adtnGetSocketIntOption(int s, int optionCode);
	public static native long adtnGetSocketLongOption(int s, int optionCode);
	public static native String adtnGetSocketStringOption(int s, int optionCode);

	public static native int adtnSendTo(int s, SockAddrT addr, byte[] data);
	public static native int adtnRecv(int s, byte[] data, int data_len);
	public static native int adtnRecvFrom(int s, byte[] data, int data_len, SockAddrT addr);

	public static final int OP_PROC_FLAGS = 1;
	public static final int OP_LIFETIME = 2;
	public static final int OP_BLOCK_FLAGS = 3;
	public static final int OP_DEST = 4;
	public static final int OP_SOURCE = 5;
	public static final int OP_REPORT = 6;
	public static final int OP_CUSTOM = 7;
	public static final int OP_LAST_TIMESTAMP = 8;
	
	
}
