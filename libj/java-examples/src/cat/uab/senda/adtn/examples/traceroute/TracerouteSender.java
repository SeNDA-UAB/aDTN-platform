/*package src.cat.uab.senda.adtn.examples.traceroute;

import src.cat.uab.senda.adtn.comm.AddressInUseException;
import src.cat.uab.senda.adtn.comm.Comm;
import src.cat.uab.senda.adtn.comm.JNIException;
import src.cat.uab.senda.adtn.comm.OpNotSuportedException;
import src.cat.uab.senda.adtn.comm.SockAddrT;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.ParameterException;

import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.text.ParseException;
import java.util.ArrayList; 
import java.util.List;
import java.util.Random;
import java.util.Scanner;


class CLA {
	@Parameter(names = {"-f", "--conf_file"}, description = "Use {config_file} config file instead of the default.")
	public String conf_file = "/etc/adtn";
	@Parameter(names = {"-s", "--size"}, description = "Set a payload of {size} bytes to ping bundles.")
	public long payload_l = 64;
	@Parameter(names = {"-l", "--lifetime"}, description = "Set {lifetime} seconds of lifetime to the tracerouted bundle.")
	public long lifetime = 30;
	@Parameter(names = {"-t", "--timeout"}, description = "Wait {timeout} seconds for custody report signals.")
	public long timeout = 30;
	@Parameter(names = {"-n", "--not-ordered"}, description = "Do not show ordered route at each custody report received.")
	public boolean order = true;
	@Parameter(description = "Destination", required=true)
	public List<String> destination = new ArrayList<String>();
	@Parameter(names = {"-h", "--help"}, help = true, description="Shows this help message.")
	public boolean help = false;
}

public class TracerouteSender {
	
	private CLA cla;
	
	private int socket;
	
	private SockAddrT source;
	
	public static void main(String[] args) {
		new TracerouteSender().GetParameters(args);
	}
	
	public void GetParameters(String[] args) {
		cla = new CLA();
		JCommander jcom = new JCommander(cla);
		jcom.setProgramName("Traceroute");
		try {
			jcom.parse(args);
			if(cla.help)
				jcom.usage();
		} catch (ParameterException ex) {
			jcom.usage();
			System.exit(0);
		}
		initSocket();
	}
	
	public void initSocket() {
		Scanner in = new Scanner(System.in);
		System.out.println("Introduce this platform name: ");
		String platform = in.nextLine();
		in.close();
		
		try {
			socket = Comm.adtnSocket();
		} catch (SocketException e) {
			e.printStackTrace();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (ParseException e) {
			e.printStackTrace();
		}
		
		int port;
		boolean usedPort = true;
		do {
			port = new Random().nextInt(Integer.MAX_VALUE) % 65536;
			source = new SockAddrT(platform, port);
			try {
				Comm.adtnBind(socket, source);
				usedPort = false;
			} catch (SocketException e) {
				e.printStackTrace();
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (IllegalAccessException e) {
				e.printStackTrace();
			} catch (AddressInUseException e) {
			}
		} while (usedPort);
		
		try {
			int flags = Comm.H_DESS | Comm.H_SR_BREC;
			Comm.adtnSetSocketOption(socket, Comm.OP_PROC_FLAGS, flags);
			Comm.adtnSetSocketOption(socket, Comm.OP_LIFETIME, cla.lifetime);
		} catch (SocketException e) {
			e.printStackTrace();
		} catch (OpNotSuportedException e) {
			e.printStackTrace();
		} catch (JNIException e) {
			e.printStackTrace();
		}
		
		sendTraceroute();
		
	}
	
	public void sendTraceroute() {
		ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
		try {
			outputStream.write(ByteBuffer.allocate(Byte.SIZE / 8).put((byte)2).array());
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	
	
}*/

