package cat.uab.senda.adtn.examples.traceroute;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.ParameterException;

import java.util.ArrayList; 
import java.util.List;


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
	
	public CLA cla;
	
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
		sendTraceroute();
	}
	
	public void sendTraceroute() {
		System.out.println(cla.destination);
		System.out.println(cla.timeout);
	}
	
	
}

