#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

using namespace std;

bool domain(vector <unsigned char> &in_buf_){
    for (int i = 0; i < 3; ++i) {
        if (in_buf_[4+i] != 0x00)
            return false;
    }
    return in_buf_[8] != 0x00;
}
string findDomain(vector <unsigned char> &in_buf_){
    string Domain;
    unsigned int pos=0;
    for (unsigned int i = 8; i < in_buf_.size(); ++i) {
        if (in_buf_[i] == 0x00){
            pos = i;
            break;
        }
    }
    cout << "pos:" << pos << endl;
    if (pos!=0){
        for (unsigned int i = pos+1; i < in_buf_.size()-1; ++i) {
            Domain += in_buf_[i];
        }
    }
    return Domain;
}

int main(){
    vector <unsigned char> in_buf_(19);
    in_buf_[0] = 0x04;
    in_buf_[1] = 0x01;
    in_buf_[2] = 0x01;
    in_buf_[3] = 0x01;
    for (int i = 0; i < 3; ++i) {
        in_buf_[i+4] = 0x00;
    }
    in_buf_[8] = 0x05;
    for (int i = 0; i < 5; ++i) {
        in_buf_[i+9] = 0x41;
    }
    in_buf_[14] = 0x00;
    for (int i = 0; i < 5; ++i) {
        in_buf_[i+15] = 0x41;
    }
    in_buf_[19] = 0x00;
    if (domain(in_buf_))
        cout << findDomain(in_buf_) << endl;
    return 0;
}