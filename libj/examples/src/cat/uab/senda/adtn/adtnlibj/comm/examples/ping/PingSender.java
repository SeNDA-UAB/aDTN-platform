package cat.uab.senda.adtn.adtnlibj.comm.examples.ping;

import cat.uab.senda.adtn.adtnlibj.comm.PlatfComm;
import cat.uab.senda.adtn.adtnlibj.comm.SockAddrT;

public class PingSender extends Thread implements Runnable {
    
    int s; // Send socket
    Configuration conf; // Configuration data
    int ping_flags;
    
    PingSender(int socket, Configuration conf){
        this.s = socket;
        this.conf = conf;
    }

    @Override
    public void run() {
        System.out.println("PING "+conf.getDest_platform_id()+" "+conf.getPayload_size()+" bytes of payload. "
                +conf.getPing_lifetime()+" seconds of lifetime.");
        
        try {
            ping_flags = PlatfComm.H_DESS | PlatfComm.H_NOTF | PlatfComm.H_SR_BREC;
            
            byte[] data = new byte[(int) conf.getPayload_size()]; //Inicialitzar be

            PlatfComm.adtnSetSocketOption(s, PlatfComm.OP_PROC_FLAGS, ping_flags);
            PlatfComm.adtnSetSocketOption(s, PlatfComm.OP_REPORT, conf.getSource());
            PlatfComm.adtnSetSocketOption(s, PlatfComm.OP_LIFETIME, conf.getPing_lifetime());
            
            // Falta Payload
            
            PlatfComm.adtnSendTo(s, conf.getDestination(), data);
        
        }catch(Exception e){
            e.printStackTrace();
        }
    }
    
}
