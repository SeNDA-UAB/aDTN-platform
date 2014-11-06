package cat.uab.senda.adtn.adtnlibj.comm;

public class SockAddrT {
	int port;
	String id;

	public SockAddrT(String id, int port){
		this.port = port;
		this.id = id;
	}
}