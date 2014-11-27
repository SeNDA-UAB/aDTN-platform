package cat.uab.senda.adtn.examples.ping;

import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.SockAddrT;

import java.util.HashMap;
import java.util.Random;
import java.util.Scanner;

public class Ping {
    public static void main(String[] args) {
        HashMap<String,String> options;
        int s,port;
        Scanner in = new Scanner(System.in);
        
        System.out.println("Introduce this platform name: ");
        String platform_id = in.nextLine();
        in.close();
        
        ArgHandler argHandler = new ArgHandler(args);
        options = argHandler.getOptions();
                
         // Set the configuration atributes
        Configuration conf = new Configuration(options.get("destination_id"),options.get("size"),options.get("count"),
                options.get("interval"),options.get("lifetime"));    
        
        //Create a random port number ToDo: Ensure that ports it's not already in use.
        port = new Random().nextInt(Integer.MAX_VALUE) % 65536;
           
        
        // Create the SockkAddr objects with the source and destination information
        conf.setSource(new SockAddrT(platform_id,port));
        conf.setDestination(new SockAddrT(options.get("destination_id"),port));
        
       // Create the aDTN socket, and bind it with the SockAddr with the source information
       s = Comm.adtnSocket();
       Comm.adtnBind(s, conf.getSource());
       
       PingSender sender = new PingSender(s, conf);
       PingReceiver receiver = new PingReceiver(s);
       
       sender.run();
       receiver.run();
       
    }

}
