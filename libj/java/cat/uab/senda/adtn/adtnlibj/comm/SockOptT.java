package cat.uab.senda.adtn.adtnlibj.comm;

public class SockOptT{
	int lifetime;
	int procFlags;
	int blockFlags;

	public SockOptT(int lifetime, int procFlags, int blockFlags){
		this.lifetime = lifetime;
		this.procFlags = procFlags;
		this.blockFlags = blockFlags;
	}

	public SockOptT(){
		this.lifetime = 86400; //One day
		this.procFlags = 0x04 | 0x10;
		this.blockFlags = 0x04;
	}

	public void setLifetime(int lifetime){
		this.lifetime = lifetime;
	}

	public void setProcFlags(int procFlags){
		this.procFlags = procFlags;
	}

	public void setBlockFlags(int blockFlags){
		this.blockFlags = blockFlags;
	}

	public int getLifetime(){
		return this.lifetime;
	}

	public int getProcFlags(){
		return this.procFlags;
	}

	public int getBlockFlags(){
		return this.blockFlags;
	}
}