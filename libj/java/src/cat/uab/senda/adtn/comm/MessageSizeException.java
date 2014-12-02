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

package src.cat.uab.senda.adtn.comm;

/**
 * Exception to control to big message errors from calls of {@link Comm}.
 * 
 * @author Senda DevLab {@literal <developers@senda.uab.cat>}
 * @version 0.3
 */
public class MessageSizeException extends Exception {

	private static final long serialVersionUID = 5154560215879270962L;

	/**
	 * Constructs a new exception with the specified detail message.
	 * 
	 * @param message - the detail message. The detail message is saved for later retrieval by the Throwable.getMessage() method.
	 */
	public MessageSizeException(String message) {
		super(message);
	}
}
