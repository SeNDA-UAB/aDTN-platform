#define R1 20

int main(int argc, char *argv[])
{
    int number_of_encountered = getState(0);
    int max_value = getState(1);
    char ritValuePath[] = "/localnode/value";
    int measured_value = ritGet(ritValuePath);

    if (number_of_encountered > R1 && measured_value > max_value) {
        setState(2, 1);
        deliverMsg();
    } else {
        setState(0, ++number_of_encountered);
    }

    return 0;
}
