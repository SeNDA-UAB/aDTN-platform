package cat.uab.senda.adtn.adtnlibj.comm.examples;

import java.io.IOException;
import java.util.Scanner;

import cat.uab.senda.adtn.adtnlibj.comm.PlatfComm;
import cat.uab.senda.adtn.adtnlibj.comm.SockAddrT;

public class Sender {
	public static void main(String[] args) throws IOException{
		Scanner in = new Scanner(System.in);
		
		System.out.println("Introduce this platform name: ");
		String platform = in.nextLine();
		System.out.println(platform);
		
		System.out.println("Introduce Receiver application platform name: ");
		String recvPlat = in.nextLine();
		
		System.out.println("Introduce Receiver application id (it is a number): ");
		int appId = in.nextInt();
		in.nextLine();
		SockAddrT receiverAddr = new SockAddrT(recvPlat, appId);
		
		System.out.println("Introduce a short text to send: ");
		String text = in.nextLine();
		
		System.out.println("Sending '"+text+"' to app "+appId+" at "+recvPlat+" platform.");
		
		int socket = PlatfComm.adtnSocket();
		PlatfComm.adtnBind(socket, new SockAddrT(platform, appId));
		PlatfComm.adtnSendTo(socket, receiverAddr, text.getBytes());
		
		System.out.println("Text '"+text+"' has been sent. Press a key to exit...");
		System.out.println();
		in.close();
	}
}