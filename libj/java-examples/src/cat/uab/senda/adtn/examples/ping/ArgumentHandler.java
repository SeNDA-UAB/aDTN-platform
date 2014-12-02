package src.cat.uab.senda.adtn.examples.ping;

import java.util.ArrayList;
import java.util.List;

import com.beust.jcommander.Parameter;

public class ArgumentHandler {

	@Parameter(names = { "-f", "--conf_file" }, description = "Use {config_file} config file instead of the default.")
	public String conf_file = "/etc/adtn/adtn.ini";
	@Parameter(names = { "-s", "--size" }, description = "Set a payload of {size} bytes to ping bundles. Mininum size is set at 14 bytes")
	public int size = 64;
	@Parameter(names = { "-l", "--lifetime" }, description = "Set {lifetime} seconds of lifetime to the tracerouted bundle.")
	public long lifetime = 30;
	@Parameter(names = { "-t", "--timeout" }, description = "Wait {timeout} seconds for custody report signals.")
	public long timeout = 30;
	@Parameter(names = { "-c", "--count" }, description = "Stop after sending {count} ping bundles.")
	public int count = -1;
	@Parameter(names = { "-i", "--interval" }, description = "Wait {interval} seconds between pings. Minimum interval is set at 1 second.")
	public int interval = 1;
	@Parameter(names = { "-m", "--mode" }, description = "Mode could be sender or receiver. Sender is set as default.")
	public String mode = "sender";
	@Parameter(names = { "-v", "--vervose" }, description = "Verbose output")
	public boolean verbose;
	@Parameter(description = "Destination")
	public List<String> destination_id = new ArrayList<String>();
	@Parameter(names = { "-h", "--help" }, help = true, description = "Shows this help message.")
	public boolean help = false;
}