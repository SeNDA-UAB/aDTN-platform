package cat.uab.senda.adtn.examples.ping;

import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.SockAddrT;

import java.util.HashMap;
import java.util.Random;
import java.util.Scanner;

public class Ping {
	public static HashMap<Integer,Long> map = new HashMap<Integer,Long>();
	
	public static void main(String[] args) {	
        HashMap<String,String> options;
        
        int s,port;
        Scanner in = new Scanner(System.in);
        
        // Ask for the platform name
        System.out.println("Introduce this platform name: ");
        String platform_id = in.nextLine();
        in.close();
        
        // Extract the parameters and store all the options
        ArgHandler argHandler = new ArgHandler(args);
        options = argHandler.getOptions();
                
        // Set the configuration attributes
        Configuration conf = new Configuration(options.get("destination_id"),options.get("size"),options.get("count"),
                options.get("interval"),options.get("lifetime"));   
        
        // Create a new aDTN socket that will be used to send and receive pings
        s = Comm.adtnSocket();
        
        // Pick a port number at Random, this port will be used to create either the SockAddrT of the source and the destination
        // If the port is already in use (the bind could not be done), we will pick another one
        while(true) {
        	//Create a random port number ToDo: Ensure that ports it's not already in use.
        	port = new Random().nextInt(Integer.MAX_VALUE) % 65536;
            
            // Create the SockkAddr objects with the source and destination information
            conf.setSource(new SockAddrT(platform_id,port));
            conf.setDestination(new SockAddrT(options.get("destination_id"),port));
            
            // Check if the port is in use
            try {
        	   Comm.adtnBind(s, conf.getSource());
        	   // If no exception is thrown, the port will be available.
	           break;
        	}catch(Exception e) {
        		//If it's in use, we notify it to the user, and try to pick another one
        		System.out.println("Port "+port+" already in use. Trying another one.");
        	}
        }
        // We create a two threads, one the the ping sender, and another for the receiver
        PingSender sender = new PingSender(s, conf);
        PingReceiver receiver = new PingReceiver(s,conf);
       
        // Start the sender and the receiver
       sender.start();
       receiver.start(); 
	}
    public static HashMap<Integer, Long> getMap() {
    	return map;
    }

}
