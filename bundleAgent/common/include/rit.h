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

/** @cond */
#ifndef INC_COMMON_RIT
#define INC_COMMON_RIT
/** @endcond */

/** 
 * @file
 *
 *  This are some functions to interact with the RIT.
 *
 *  Some of this functions are necessary for different types of applications:
 *
 *  If you want to use multicast, you will need to subscribe into a group, to do this you will need to add the groups as values in the 
 *  path <nodeId>/subscribed
 *
 *  The same goes if you want to announce something with the RIT.
 *
 *  To announce anything, you must add it into the RIT, then you need to add the TAG "announceable" with a value of "1", with this
 *  your path with it's values is going to be announced with beacons to the neighbours.
 *
 *  To update the new values, it's needed to activate the signal SIGUSR2 to the information_exchange process.
 *  This can be done of differents forms, one of them using the kill function as follows: 
 *
 *  kill -s SIGUSR2 <pid>
 */

//rit.h

/** @cond */
typedef struct{
	int transaction;
}transaction_args;
/** @endcond */

/** @cond */
/* VARIABLE FUNCTIONS */
#ifndef DOX
#define rit_set(path, value, ...) rit_var_set(path, value, ##__VA_ARGS__) 
#define rit_unset(path, ...) rit_var_unset(path, ##__VA_ARGS__) 
#define rit_tag(path, name, value, ...) rit_var_tag(path, name, value, ##__VA_ARGS__) 
#define rit_untag(path, name, ...) rit_var_untag(path, name, ##__VA_ARGS__) 
#define rit_clear(path, ...) rit_var_clear(path, ##__VA_ARGS__) 
#define rit_delete(path, ...) rit_var_delete(path, ##__VA_ARGS__) 
#endif
/** @endcond */

#ifdef DOX
int rit_set(const char* path, const char* value);
int rit_unset(const char* path);
int rit_tag(const char* path, const char* name, const char* value);
int rit_untag(const char* path, const char* name);
/** @cond */
int rit_clear(const char* path);
int rit_delete(const char* path);
/** @endcond */
#endif

/** @cond */
int rit_var_set(const char* path, const char* value, ...);
int rit_var_unset(const char* path, ...);
int rit_var_tag(const char* path, const char* name, const char* value, ...);
int rit_var_untag(const char* path, const char* name, ...);
int rit_var_clear(const char* path, ...);
int rit_var_delete(const char *path, ...);
/** @endcond */

/** @cond */
int rit_changePath(const char *newPath);
void rit_drop();
int rit_start();
int rit_commit();
int rit_rollback();
/** @endcond */

char* rit_getValue(const char *path);
char* rit_getTag(const char *path, const char *nom);

/** @cond */
char* rit_getPathsByTag(const char *nom, const char* value);
char* rit_getTagNamesByPath(const char *path);
char* rit_getChilds(const char *path);
/** @endcond */

/** @cond */
#endif
/** @endcond */

/**
 * @fn int rit_set(const char* path, const char* value)
 * @brief Sets a path with the given value.
 *
 * This function lets to add a new path into the RIT, and at the same time add one value into it.
 *
 * This function it's also usefull to add a value to an existing path.
 *
 * @param path The path into the RIT.
 * @param value The new value to set.
 *
 * @return This functions returns 1 on error, other value otherwise.
 */

/**
 * @fn int rit_unset(const char* path)
 * #brief Removes the values of the given path.
 *
 * This function removes all the values of the given path.
 * 
 * @param  path The path into the RIT.
 * @return This functions returns 1 on error, other value otherwise.
 */

/**
 * @fn int rit_tag(const char* path, const char* name, const char* value)
 * @brief This function adds a tag to the given path with the given value.
 *
 * This function adds the given tag, to the given path, with the given value.
 *
 * This is usefull to announce something of your RIT to the world.
 *
 * To do this the "announceable" tag must be add to the path with a value of "1".
 * 
 * @param  path  The path into the RIT.
 * @param  name  The name of the tag.
 * @param  value The value of the tag.
 * @return This functions returns 1 on error, other value otherwise.
 */

/**
 * @fn int rit_untag(const char* path, const char* name)
 * @brief This function removes a tag.
 *
 * Given a path into the RIT this function removes the tags, if any, that could have.
 *
 * @param  path The path into the RIT.
 * @param  name The name of the tag.
 * @return This functions returns 1 on error, other value otherwise.
 */

/**
 * @fn char* rit_getValue(const char *path)
 * @brief This functions gets the value of the given path.
 *
 * This is a usefull function, it can be used into the forwarding, life and priority codes, letting the option of 
 * change the result based on values given by the neighbour beacons.
 * 
 * @param  path The path into the RIT.
 * @return This functions returns 1 on error, other value otherwise.
 */

/**
 * @fn char* rit_getTag(const char *path, const char *nom)
 * @brief This functions gets the value of the tag into the given path.
 *
 * This is a usefull function, it can be used into the forwarding, life and priority codes, letting the option of 
 * change the result based on values of the tags given by the neighbour beacons.
 * 
 * @param  path The path into the RIT.
 * @param  nom  The name of the tag.
 * @return This functions returns 1 on error, other value otherwise.
 */