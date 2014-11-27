package cat.uab.senda.adtn.examples.basic;

import java.io.IOException;
import java.util.Scanner;

import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.SockAddrT;

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
		
		int socket = Comm.adtnSocket();
		Comm.adtnBind(socket, new SockAddrT(platform, appId));
		Comm.adtnSendTo(socket, receiverAddr, text.getBytes());
		
		System.out.println("Text '"+text+"' has been sent. Press a key to exit...");
		System.out.println();
		in.close();
	}
}