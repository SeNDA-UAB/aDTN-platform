package cat.uab.senda.adtn.adtnlibj;

import cat.uab.senda.adtn.adtnlibj.comm.SockAddrT;

public class ADTNAddress {

	private String address;
	private int appId;
	
	public ADTNAddress(String address, int appId) {
		this.address = address;
		this.appId = appId;
	}

	public String getAddress() {
		return address;
	}

	public int getAppId() {
		return appId;
	}

	public SockAddrT toSockAddrT(){
		return new SockAddrT(this.address, this.appId);
	}
	
	public String toString(){
		return address + ":" + appId;
	}
}
