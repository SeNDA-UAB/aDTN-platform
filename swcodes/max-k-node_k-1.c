#define R1 20

int main(int argc, char *argv[])
{   
    int forwarded = getState(0);
    int max = getState(1);
    char ritValuePath[] = "/localnode/value";
    int localvalue = ritGet(ritValuePath);

    if (forwarded > R1 && localvalue > max) {
        deliverMsg();
    } else {
        setState(0 , ++forwarded);
        setState(1 ,  MAX(max, localvalue));
    }

    return 0;
}
