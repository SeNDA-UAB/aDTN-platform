package cat.uab.senda.adtn.examples.ping;

public class PingReceiver extends Thread implements Runnable{
    
    int s; // Receive socket
    
    PingReceiver(int socket){
        this.s = socket;
    }
    
     @Override
    public void run() {
        System.out.println("Running Receiver on socket "+Integer.toString(s));
    }
}
