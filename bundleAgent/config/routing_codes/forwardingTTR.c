#include "adtnrhelper.h"
#include "adtnrithelper.h"

// Routing algorithm
int routing(char *dest)
{
	nbs.start();
	//Starts code to include in the bundle
	int minttr = 100; // maxTTR=100
	int ttr = 0;
	int len;
	while (nbs.has_next()) {
		char triageBranch[160] = {0};
		char *endPointId = nbs.next();
		snprintf(triageBranch, 159, "/%s/TTR", endPointId);
		ttr = atoi(getValue(triageBranch));
		if (ttr < minttr) {
			minttr = ttr;
			cln_hops();
			add_hop(endPointId);
		} else if (ttr == minttr) {
			add_hop(endPointId);
		}
		add_hop(nbs.next());
	}
	//Ends code to include in the bundle
	return 0;
};


int main(int argc, char const *argv[])
{
	
	routing("dest-platform");

	return 0;
}