package cat.uab.senda.adtn.adtnlibj;

import cat.uab.senda.adtn.adtnlibj.comm.PlatfComm;
import cat.uab.senda.adtn.adtnlibj.comm.SockAddrT;

public class ADTNSocket {

	private static final String DEFAULT_ADTN_INIT_FILE_PATH = "/etc/adtn/adtn.ini";
	
	private int socket;
	
	public ADTNSocket(int appId){
		this(appId, DEFAULT_ADTN_INIT_FILE_PATH);
	}
	
	public ADTNSocket(int appId, String adtnIniFilePath){
		socket = PlatfComm.adtnSocket(adtnIniFilePath);
		//TODO: read platform name from adtn.ini
		String platform = "platform1";
		SockAddrT platformAddress = new SockAddrT(platform, appId);
		PlatfComm.adtnBind(socket, platformAddress);
	}
	
	public int send(ADTNAddress destination, byte[] data){
		return PlatfComm.adtnSendTo(socket, destination.toSockAddrT(), data);
	}
	
	public int receive(byte[] data, int length){
		return PlatfComm.adtnRecv(socket, data, length);
	}
	
	public int receive(ADTNAddress from, byte[] data, int length){
		return PlatfComm.adtnRecvFrom(socket, data, length, from.toSockAddrT());
	}
	
	public void close(){
		PlatfComm.adtnClose(socket);
	}
	
	public void shutdown(){
		PlatfComm.adtnShutdown(socket);
	}
}
