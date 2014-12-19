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

package cat.uab.senda.adtn.comm;

/**
* This class represents a socket address over the aDTN.
*
*
* @author Senda DevLab {@literal <developers@senda.uab.cat>}
* @version 0.1
*
*/
public class SockAddrT {
	/**
	 * Port of the application.
	 */
	int port;
	/**
	 * Identifier of the platform.
	 */
	String id;
	/**
	 * Create a new socket address.
	 *
	 * @param  id  	identifier of the platform. 
	 * @param  port port of the application.
	 */
	public SockAddrT(String id, int port){
		this.port = port;
		this.id = id;
	}
	/**
	 * Returns the socket address as string.
	 *
	 * The string has the format {@code id:port}.
	 * @return the string with the socket address.
	 */
	public String toString(){
		return id + ":" + port;
	}
	/**
	 * @return the port
	 */
	public int getPort() {
		return port;
	}
	/**
	 * @return the id
	 */
	public String getId() {
		return id;
	}
	
}