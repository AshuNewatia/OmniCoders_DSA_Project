    #include <iostream>
    #include <vector>
    #include <string>
    #include <stdexcept>
    #include <algorithm>
    int CAP = 1024;

    //Capacity is about twice the maximum expected. Default = 1024, Given 512 basic slots.
    struct twin{
        int ID;
        int address ;
        twin(int ID, int i = -1): ID(ID), address(i){};
    };

    twin e(-1, -1);


    // RULES:  "NA" is -1 "DELETED" is -2
    // e: Identity Element
    // ID can never be NA or DELETED


    class LookUp{
        private:
        std::vector<twin> tab;
        int state_count = 0;
        //Hash used here is Polynomial.
        long long hashie (int input){
            std::string input_str = std::to_string(input);
            int n = input_str.length();
            long long hash = 0;
            int base = 31; //idk
            for(int i = 0; i < n; i++){
                hash = hash*base + int(input_str[i]);
            }
            return hash;
        }

        void resize(){
            std::vector<twin> old_tab = tab;
            int old_CAP = CAP;
            CAP = 2*CAP;
            tab.assign(CAP, e);
            state_count = 0;
            for(int i = 0; i < old_CAP; i++){
                if(old_tab[i].ID != -1 && old_tab[i].ID != -2){
                    int put = hashie(old_tab[i].ID) % CAP;
                    while(tab[put].ID != -1){
                        put = (put + 1) % CAP;
                    }
                    tab[put] = old_tab[i];
                    state_count++;
                }
            }
        }

        public:
        LookUp(){
            tab.resize(CAP, e);
        }

        void Insert (twin in){
            if(state_count > CAP-1){
                resize();
            }
            long long hash = hashie(in.ID);
            int put = hash%CAP;
            int pu1 = put;
            while(tab[pu1].ID != -1){
                if(tab[pu1].ID == in.ID){
                    tab[pu1].address = in.address;
                    return;
                }
                pu1 = (pu1+1)%CAP;
            }
            while(tab[put].ID != -1 && tab[put].ID != -2){
                put = (put+1)%CAP;
            }
            state_count++;
            tab[put].ID = in.ID;
            tab[put].address = in.address;
        }


        //Skipping deleting checks, coz this is personalised

        int find(int ID){
            long long hash = hashie(ID);
            int ch = hash%CAP;
            while(tab[ch].ID!=ID && tab[ch].ID != -1){
                ch = (ch+1)%CAP;
            }
            if(tab[ch].ID != ID) {
                throw std::out_of_range("Not Found");
            }
            return tab[ch].address;
        }

        void Delete(int in){
            long long hash = hashie(in);
            int ch = hash%CAP;
            while(tab[ch].ID!= in && tab[ch].ID != -1){
                ch = (ch+1)%CAP;
            }
            if(tab[ch].ID != in) {
                throw std::out_of_range("Not Found");
                return;
            }
            tab[ch].address = -1;
            tab[ch].ID = -2;
            state_count--;
        }


        void interchange (int  f, int s){
            long long hash = hashie(f);
            int ch = hash%CAP;
            while(tab[ch].ID!=f){
                ch = (ch+1)%CAP;
            }
            int first = ch;

            hash = hashie(s);
            ch = hash%CAP;
            while(tab[ch].ID!=s){
                ch = (ch+1)%CAP;
            }
            int second = ch;

            std::swap(tab[first].address, tab[second].address);
        }
    };
