package src.cat.uab.senda.adtn.examples.ping;

import java.util.HashMap;

public class ArgHandler {

	// Default values
	private static final String DEFAULT_CONF_FILE = "/etc/adtn/adtn.ini";
	private static final String DEFAULT_LIFETIME = "30";
	private static final String DEFAULT_PAYLOAD_SIZE = "64";
	private static final String DEFAULT_INTERVAL = "1000"; // Milliseconds
	private static final String DEFAULT_COUNT = "-1"; // Never stops
	private static final String DEFAULT_MODE = "sender"; // If 0 the application only
													// receive Pings, by default
													// its bidirectional
	private static final String DEFAULT_DEST = "NULL"; // No destination
														// specified

	private HashMap<String, String> options;

	ArgHandler(String[] args) {
		options = new HashMap<>();
		String id = "";
		int index;

		// Set the default values to the configuration variables.
		this.init();

		for (String arg : args) {
			switch (arg.charAt(0)) {
			case '-':
				index = 1;
				if (arg.length() < 2)
					throw new IllegalArgumentException("Not a valid argument "
							+ arg);
				if (arg.charAt(1) == '-') {
					index = 2;
					if (arg.length() < 3)
						throw new IllegalArgumentException(
								"Not a valid argument " + arg);
				}
				/* Normalize the params identifier */

				switch (arg.substring(index)) {
				case "conf_file":
				case "f":
					id = "conf_file";
					break;
				case "size":
				case "s":
					id = "size";
					break;
				case "lifetime":
				case "l":
					id = "lifetime";
					break;
				case "count":
				case "c":
					id = "count";
					break;
				case "interval":
				case "i":
					id = "interval";
					break;
				case "mode":
				case "m":
					id = "mode";
					break;
				case "help":
				case "h":
					System.out
							.println("Adtn-ping is part of the SeNDA aDTN platform\n"
									+ "Usage: Adtn-ping [options] [platform_id]\n"
									+ "Supported options:\n"
									+ "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at /etc/adtn\n"
									+ "       [-s | --size] size\t\t\tSet a payload of {size} bytes to ping bundles.\n"
									+ "       [-l | --lifetime] lifetime\t\tSet {lifetime} seconds of lifetime to the ping bundles.\n"
									+ "       [-c | --count] count\t\t\tStop after sending {count} ping bundles.\n"
									+ "       [-i | --interval] interval \t\tWait {interval} milliseconds between pings.\n"
									+ "       [-m | --mode] \t\t\t\tMode could be sender or receiver. Sender is set as default.\n"
									+ "       [-h | --help]\t\t\t\tShows this help message.\n");
					System.exit(0);

				}
				break;
			default:
				/* Ensure that the passed argument is valid (non 0) */
				if (!id.equals("")) {
					if ((id.equals("size") || id.equals("lifetime")
							|| id.equals("count") || id.equals("interval") || id.equals("mode"))
							&& !arg.equals("0")) {
						options.put(id, arg);
						id = "";
					}
				} else {
					options.put("destination_id", arg);
				}
				break;
			}

		}

		if (options.get("destination_id").equals("NULL") && options.get("mode").equals("sender")) {
			System.out.println("You must specify a destination.");
			System.exit(-1);
		}
	}

	public final void init() {
		options.put("conf_file", DEFAULT_CONF_FILE);
		options.put("size", DEFAULT_PAYLOAD_SIZE);
		options.put("lifetime", DEFAULT_LIFETIME);
		options.put("count", DEFAULT_COUNT);
		options.put("interval", DEFAULT_INTERVAL);
		options.put("mode", DEFAULT_MODE);
		options.put("destination_id", DEFAULT_DEST);
	}

	public HashMap<String, String> getOptions() {
		return this.options;
	}
}
