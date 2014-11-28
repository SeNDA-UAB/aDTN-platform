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

package cat.uab.senda.adtn.examples.ping;

import java.util.HashMap;

public class ArgHandler {
    
    //Default values
    private static final String DEFAULT_CONF_FILE = "/etc/adtn/adtn.ini";
    private static final String DEFAULT_LIFETIME = "30";
    private static final String DEFAULT_PAYLOAD_SIZE = "64";
    private static final String DEFAULT_INTERVAL = "1000"; // Milliseconds
    private static final String DEFAULT_COUNT = "-1"; // Never stops
  
    private HashMap<String,String> options;
    
    ArgHandler(String[] args){
       options = new HashMap<>();
       String id = "";
       int index;
       
       // Set the default values to the configuration variables.
       this.init();
       
       for(String arg: args){
           switch(arg.charAt(0)) {
               case '-':
                   index = 1;
                   if (arg.length() < 2)
                       throw new IllegalArgumentException("Not a valid argument "+arg);
                   if (arg.charAt(1) == '-') {
                       index = 2;
                       if (arg.length() < 3)
                           throw new IllegalArgumentException("Not a valid argument "+arg);
                   }
                   /* Normalize the params identifier */
                   
                   switch(arg.substring(index)) { 
                       case "conf_file":
                       case "f":
                            id = "conf_file";  
                            break;
                       case "size":
                       case "s":
                            id = "size" ;
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
                       case "help":
                       case "h":
                           System.out.println(  "Adtn-ping is part of the SeNDA aDTN platform\n"+
                                                "Usage: Adtn-ping [options] [platform_id]\n"+
                                                "Supported options:\n"+
                                                "       [-f | --conf_file] config_file\t\tUse {config_file} config file instead of the default found at /etc/adtn\n"+
                                                "       [-s | --size] size\t\t\tSet a payload of {size} bytes to ping bundles.\n"+
                                                "       [-l | --lifetime] lifetime\t\tSet {lifetime} seconds of lifetime to the ping bundles.\n"+
                                                "       [-c | --count] count\t\t\tStop after sending {count} ping bundles.\n"+
                                                "       [-i | --interval] interval \t\tWait {interval} milliseconds between pings.\n"+
                                                "       [-h | --help]\t\t\t\tShows this help message.\n");
                           System.exit(0);
                       
                   }
                   break;
               default:
                   /* Ensure that the passed argument is valid (non 0) */
                   if (!id.equals("")) {
                       if((id.equals("size") || id.equals("lifetime") || id.equals("count") || id.equals("interval")) && !arg.equals("0")){
                           options.put(id, arg);
                           id = "";
                       }
                   }else{
                      options.put("destination_id", arg); 
                   }
                   break;
           }
            
        }
    }
    
    public final void init(){
        options.put("conf_file", DEFAULT_CONF_FILE);
        options.put("size", DEFAULT_PAYLOAD_SIZE);
        options.put("lifetime", DEFAULT_LIFETIME);
        options.put("count", DEFAULT_COUNT);
        options.put("interval", DEFAULT_INTERVAL);
    }
    
    public HashMap<String,String> getOptions() {
        return this.options;
    }
}
