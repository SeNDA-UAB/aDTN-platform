package src.cat.uab.senda.adtn.examples.ping;

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Date;

import src.cat.uab.senda.adtn.comm.Comm;
import src.cat.uab.senda.adtn.comm.InvalidArgumentException;
import src.cat.uab.senda.adtn.comm.JNIException;
import src.cat.uab.senda.adtn.comm.MessageSizeException;
import src.cat.uab.senda.adtn.comm.OpNotSuportedException;

public class PingSender extends Thread implements Runnable {
	
	// Protocol opcode, Ping is #1
	public static byte TYPE = 1;
	//Option, specify if the message is a Ping or a Pong
	public static byte PING = 0; 
	public static byte PONG = 1;
    
    int s; // Send socket
    Configuration conf; // Configuration data
    int ping_flags, seq_num;
    long time;
    
    
    
    PingSender(int socket, Configuration conf){
        this.s = socket;
        this.conf = conf;
    }

    @Override
    public void run() {
    	seq_num = 1;
    	ping_flags = Comm.H_NOTF | Comm.H_DESS;
       
        try {
			Comm.adtnSetSocketOption(s, Comm.OP_PROC_FLAGS, ping_flags);
			Comm.adtnSetSocketOption(s, Comm.OP_REPORT, conf.getSource());
		    Comm.adtnSetSocketOption(s, Comm.OP_LIFETIME, conf.getPing_lifetime());
		    
		    System.out.println("PING "+conf.getDest_platform_id()+" "+conf.getPayload_size()+" bytes of payload. "
	                +conf.getPing_lifetime()+" seconds of lifetime.");
		    
	    	while(true) {
	            byte[] data = new byte[conf.getPayload_size()];
	      
	            ByteBuffer buff = ByteBuffer.wrap(data);
	            
	            buff.put(TYPE);
	            buff.put(PING);
	            buff.putInt(seq_num);

	            time = new Date().getTime();
	            buff.putLong(time);
	            Ping.getMap().put(seq_num, time);
	            
	            Comm.adtnSendTo(s, conf.getDestination(), data);
	            
	            seq_num++;
		        
	            sleep(conf.getPing_interval());
	    	}
        } catch (SocketException | OpNotSuportedException | JNIException | FileNotFoundException | InvalidArgumentException | MessageSizeException | InterruptedException e1) {
			e1.printStackTrace();
		}
    }
    
}
