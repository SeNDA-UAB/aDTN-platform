/*
* Copyright (c) 2014 SeNDA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* 
*/

package cat.uab.senda.adtn.examples.basic;

import java.io.IOException;
import java.net.SocketException;
import java.text.ParseException;
import java.util.Scanner;

import cat.uab.senda.adtn.comm.AddressInUseException;
import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.InvalidArgumentException;
import cat.uab.senda.adtn.comm.MessageSizeException;
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
		
		int socket;
		try {
			socket = Comm.adtnSocket();
			Comm.adtnBind(socket, new SockAddrT(platform, appId));
			Comm.adtnSendTo(socket, receiverAddr, text.getBytes());
			Comm.adtnClose(socket);
		} catch (ParseException | IllegalAccessException | AddressInUseException | InvalidArgumentException | MessageSizeException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		System.out.println("Text '"+text+"' has been sent. Press a key to exit...");
		System.out.println();
		in.close();
	}
}