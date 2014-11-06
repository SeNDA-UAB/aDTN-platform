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
	public static native void adtnSetSocketOption(int s, SockOptT options);

	public static native int adtnSendTo(int s, SockAddrT addr, byte[] data);
	public static native int adtnRecv(int s, byte[] data, int data_len);
	public static native int adtnRecvFrom(int s, byte[] data, int data_len, SockAddrT addr);
}
