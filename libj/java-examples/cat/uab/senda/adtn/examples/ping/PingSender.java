package cat.uab.senda.adtn.examples.ping;

import cat.uab.senda.adtn.comm.Comm;

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
            ping_flags = Comm.H_DESS | Comm.H_NOTF | Comm.H_SR_BREC;
            
            byte[] data = new byte[(int) conf.getPayload_size()]; //Inicialitzar be

            Comm.adtnSetSocketOption(s, Comm.OP_PROC_FLAGS, ping_flags);
            Comm.adtnSetSocketOption(s, Comm.OP_REPORT, conf.getSource());
            Comm.adtnSetSocketOption(s, Comm.OP_LIFETIME, conf.getPing_lifetime());
            
            // Falta Payload
            
            Comm.adtnSendTo(s, conf.getDestination(), data);
        
        }catch(Exception e){
            e.printStackTrace();
        }
    }
    
}
