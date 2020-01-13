#include <cstdlib>
#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <fstream>
#include <vector>
#include <sstream>

using namespace std;

bool checkIP(string IP){
    string hostIP = "140.113.87.19";
    for (int i = 0; i < 4; ++i) {
        size_t pos, pos2;
        string tmp, tmp2;
        if (i!=3){
            pos = IP.find('.');
            tmp = IP.substr(0, pos);
            pos2 = hostIP.find('.');
            tmp2 = hostIP.substr(0, pos);
        } else{
            tmp = IP;
            tmp2 = hostIP;
        }

        if (tmp != tmp2 && tmp != "*")
            return false;

        if (i != 3){
            IP = IP.substr(pos+1);
            hostIP = hostIP.substr(pos2+1);
        }
    }
    return true;
}

bool firewall(){
    vector<unsigned char> in_buf_(8);
    in_buf_[1] = 0x01;
    ifstream _in("socks.conf");
    string line;
    while (getline(_in, line)){
        istringstream iss(line);
        string permit, mode, IP;
        iss >> permit >> mode >> IP;

        if ( mode == "c" && in_buf_[1] == 0x01 && checkIP(IP) ){
            return true;
        } else if( mode == "b" && in_buf_[1] == 0x02 && checkIP(IP) ){
            return true;
        } else{
            continue;
        }
    }
    return false;
}

int main(){
    if (firewall())
        cout << "Accept" << endl;
    else
        cout << "Reject" << endl;
}