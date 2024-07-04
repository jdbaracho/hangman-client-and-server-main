#include <sys/stat.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <cctype>
#include <unistd.h>
#include <sstream>
#include <string>
#include <cstdio>
#include <unordered_set>
#include <dirent.h>

#define DEFAULT_GS_PORT "58019" // 58000 + Group Number (19)
#define TCP_CMD_CHAR 11
#define WRONG_INPUT_ERROR_MSG "Wrong Input. Usage: ./GS words_file.txt [-p Port] [-v]\n"

std::string word_file; // name of the words file
std::string gs_port; // GSport
bool verbose_mode;

int server_udp_socket_fd;
int server_tcp_socket_fd;

int word_index = 0;

struct addrinfo *udp_result, *tcp_result;
struct sockaddr_in udp_addr, tcp_addr;

std::vector <std::vector <std::string> > words;

void print_if_verbose(std::string toprint){
	if(verbose_mode){
		std::cout << toprint << "\n";
	}
}

inline bool file_exists (const std::string& file_name) {
    struct stat buffer;   
    return (stat (file_name.c_str(), &buffer) == 0); 
}

void populate_words_list(std::string file_name){
    std::ifstream file (word_file);
    for(std::string line; getline( file, line );){
        std::vector<std::string> v;
        std::string word;
        std::stringstream ss(line);
        while(getline(ss, word, ' ')) {
            v.push_back(word);
        }
        words.push_back(v);
    }
}

// returns true if successful, false if an error ocurred
bool handle_server_input(int argc, char const **argv){

    switch(argc) {

		case 2:
			word_file = argv[1];

			if(!file_exists(word_file)){ return false; }

			gs_port = DEFAULT_GS_PORT;
			verbose_mode = false;

			return true;

		case 3:
			word_file = argv[1];

			if(!file_exists(word_file)){ return false; }
			if(strcmp(argv[2], "-v")) { return false; }

			gs_port = DEFAULT_GS_PORT;
			verbose_mode = true;

			return true;

		case 4:
			word_file = argv[1];
			gs_port = argv[3]; 

			if(!file_exists(word_file)){ return false; }
			if(strcmp(argv[2], "-p")){ return false; }

			verbose_mode = false;

		return true; 

		case 5:
			word_file = argv[1];

			if(!file_exists(word_file)){ return false; }

			if(!strcmp(argv[2], "-p") && !strcmp(argv[4], "-v")) {
				gs_port = argv[3]; 
				verbose_mode = true;
				return true;
			}
			
			if(!strcmp(argv[3], "-p") && !strcmp(argv[2], "-v")) {
				gs_port = argv[4]; 
				verbose_mode = true;
				return true;
			}

			return false;

		default:
			return false;
	}
}

void setup_sockets(){
    int port = stoi(gs_port);
    struct addrinfo udp_hints, tcp_hints;
    int err;
    socklen_t addr_length;
    char buffer[50];

    server_udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_udp_socket_fd < 0) exit(1);

    memset(&udp_hints, 0, sizeof udp_hints);
    udp_hints.ai_family = AF_INET;
    udp_hints.ai_socktype = SOCK_DGRAM;
    udp_hints.ai_flags = AI_PASSIVE;

    err = getaddrinfo(NULL, gs_port.c_str() , &udp_hints, &udp_result);
    if(err != 0){ std::cout << "error: getaddrinfo line 126\n" ; exit(1) ;}

    err = bind(server_udp_socket_fd, udp_result->ai_addr, udp_result->ai_addrlen);
    if(err == -1) { std::cout << "error: udp bind line 129\n" ; exit(1) ;}
	
	// TCP socket
	server_tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_tcp_socket_fd == -1) { std::cout << "error: socket line 133\n"; return; }

	memset(&tcp_hints, 0, sizeof tcp_hints);
	tcp_hints.ai_family = AF_INET;
	tcp_hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(NULL, gs_port.c_str(), &tcp_hints, &tcp_result);
	if(err != 0) { std::cout << "error: getaddrinfo line 140\n"; return; }

    err = bind(server_tcp_socket_fd, tcp_result->ai_addr, tcp_result->ai_addrlen);
    if(err == -1) { std::cout << "error: tcp bind line 143\n" ; exit(1) ;}

    if(listen(server_tcp_socket_fd, 5) == -1){ std::cout << "error: listen line 146\n" ; exit(1);}
}

void close_sockets(){
    freeaddrinfo(udp_result);
    freeaddrinfo(tcp_result);
    close(server_udp_socket_fd);
    close(server_tcp_socket_fd);
}

bool has_ongoing_game(std::string player_id){
	std::string filename = "GAME_" + player_id + ".txt";
    return file_exists(filename);
}

// true if string is a number
bool is_number(std::string s) {
    for(char c: s) {
        if(!isdigit(c)) { return false; }
    }
    return true;
}

int calculate_max_errors(int len){
	if(len <= 6){
		return 7;
	} else if(len >= 7 && len <= 12){
		return 8;
	} else if(len >= 11){
		return 9;
	} else {
		return 0;
	}
}

std::vector<std::string> choose_word_random(){
	std::vector<std::string> result;

	result.push_back(words[word_index][0]);
	result.push_back(words[word_index][1]);
	if(word_index == words.size()){
		word_index = 0;
	} else {
		word_index++;
	}

	return result;
}

inline void send_udp_response(std::string tosend){
	int n;
	socklen_t addrlen = sizeof(udp_addr);
	n = sendto(server_udp_socket_fd, tosend.c_str(), strlen(tosend.c_str()), 0, (struct sockaddr*) &udp_addr, addrlen);
	if(n == -1) {std::cout << "error: sednto line 206\n"; exit(1);}
}

int get_current_trial(std::string player_file){
	int number_of_lines = 0;
    std::string line;
    std::ifstream file(player_file);

    while (std::getline(file, line))
        ++number_of_lines;
	
	return number_of_lines;
}

bool has_played_before(std::string player_id, std::string letter_or_word){
	std::string player_file_name = "GAME_" + player_id + ".txt";

	std::ifstream file (player_file_name);
    for(std::string line; getline( file, line );){
        std::string word;
        std::stringstream ss(line);
		getline(ss, word, ' ');
		getline(ss, word, ' ');
        if(word == letter_or_word){
			return true;
		}
    }
	return false;
}

std::string get_current_word(std::string filename){
	std::string line, word;
	std::ifstream file(filename);

	getline(file,line);
	std::stringstream ss(line);
	getline(ss, word, ' ');
	return word;
}

bool check_letter_in_string(std::string word, char letter){
	for(int i = 0; i < word.size(); i++){
		if(word[i] == std::tolower(letter)){
			return true;
		}
	}
	return false;
}

bool check_word_equal(std::string word, std::string compare){
	return (!strcasecmp(word.c_str(), compare.c_str()));
}

std::string get_positions_in_word(std::string word, std::string letter){
	char c = letter.c_str()[0];
	std::string result = "";
	int counter = 0;
	for(int i = 0; i < word.size(); i++){
		if(word[i] == std::tolower(c)){
			result += std::to_string(i+1) + " ";
			counter++;
		}
	}

	return std::to_string(counter) + " " + result;
}

void handle_start(std::vector <std::string> arguments){
    std::string player_id = arguments[1];
	std::vector <std::string> word_and_hint = choose_word_random();
	std::string player_file_name = "GAME_" + player_id + ".txt";
	std::string n_letters = std::to_string(word_and_hint[0].size());
	std::string max_errors = std::to_string(calculate_max_errors(word_and_hint[0].size()));

    if(arguments.size() != 2 || arguments[1].size() != 6 || !is_number(arguments[1])){
        send_udp_response("SNG ERR\n");
        return;
    }

    if(has_ongoing_game(player_id)){ 
		send_udp_response("RSG NOK\n");
		return;	
	}

    std::ofstream session(player_file_name);
	session << word_and_hint[0] << " " << word_and_hint[1] << "\n";

	std::string response = "RSG OK " + n_letters + " " + max_errors + "\n";

	send_udp_response(response); // OK, select word, send number of letters and send max errors
}

int get_current_errors_or_success(std::string filename, std::string err_succ){
	int n_errors = 0;
	int n_succ = 0;
	std::string current_word = get_current_word(filename);
	std::ifstream file (filename);
    for(std::string line; getline( file, line );){
        std::string word;
        std::stringstream ss(line);
        getline(ss, word, ' ');
		if(word == "T"){
			getline(ss, word, ' ');
			if(!check_letter_in_string(current_word, std::tolower(word[0]))){
				n_errors++;
			} else {
				n_succ++;
			}
		}
		if(word == "G"){
			getline(ss, word, ' ');
			if(!check_word_equal(current_word, word)){
				n_errors++;
			} else {
				n_succ++;
			}
		} 
    }

	if(err_succ == "success"){
		return n_succ;
	}

	return n_errors;
}

int get_unique_chars(std::string word){
    std::unordered_set<char> s;
    for (int i = 0; i < word.size(); i++) {
        s.insert(word[i]);
    }
    return s.size();
}

bool is_win(std::string filename, std::string current_word){
	// get number of unique characters in word
	int chars = get_unique_chars(current_word);
	// check how many characters in the list belong to the word
	int n_belongs = get_current_errors_or_success(filename, "success");
	// check if that number = number of unique, if yes then it's a win
	return n_belongs == chars;
}

std::string get_current_time(){
	std::string result;

	time_t rawtime;	
	struct tm * timeinfo;
  	char buffer [80];
  	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	strftime (buffer, 80,"%d%m%Y_%H%M%S", timeinfo);
	std::string date_and_time(buffer);

	result = date_and_time;
	return result;
}

// TODO IMPLEMENT
void mark_as_done(std::string filename, std::string player_id, std::string exit_status){
	std::string new_name = get_current_time() + "_" + exit_status + ".txt";

	std::rename(filename.c_str(), new_name.c_str());
	std::string pathname = "GAMES/" + player_id + "/";
	std::string command = "mv " + new_name + " " + pathname;
	struct stat info;
	stat(pathname.c_str(), &info);
	if(info.st_mode & S_IFDIR) {
    	system(command.c_str());
	} else {
    	mkdir(pathname.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		system(command.c_str());
	}
}

void create_score_file(std::string filename, std::string player_id, std::string word){
	float n_succ = get_current_errors_or_success(filename, "success");
	float n_trials = get_current_trial(filename) - 1;
	float score = round( (n_succ/n_trials) * 100);

	std::string s_filename = std::to_string(int(score)) + "_" + player_id + "_" + get_current_time() + ".txt";
	std::string file_content = std::to_string(int(score)) + " " + player_id + " " + word + " " + std::to_string(int(n_succ)) + " " + std::to_string(int(n_trials)) + "\n";
	
	std::ofstream scorefile("SCORES/" + s_filename);
	scorefile << file_content;
	scorefile.close();
}

void handle_play(std::vector <std::string> arguments){
    std::string player_id = arguments[1];
    std::string letter = arguments[2];
    std::string trial = arguments[3];
    std::string player_file_name = "GAME_" + player_id + ".txt";
	std::string word = get_current_word(player_file_name);
	int current_trial = get_current_trial(player_file_name);
	int max_errors = calculate_max_errors(word.size());
	std::string response;
	
	if(arguments.size() != 4 || player_id.size() != 6 || !is_number(player_id) || !is_number(trial)){
        send_udp_response("RLG ERR\n");
        return;
    }

	if(!has_ongoing_game(player_id)){ 
		send_udp_response("RLG ERR\n");
		return;
	}

	if(has_played_before(player_id, letter) && trial == std::to_string(current_trial-1)){
		// SEND RESPONSE AGAIN: no idea how ;-;
	}

	if(current_trial != stoi(trial)){
		send_udp_response("RLG INV " + std::to_string(current_trial-1) + "\n");
		return;
	}

	if(has_played_before(player_id, letter)){
		int previous_trial = stoi(trial) - 1;
		send_udp_response("RLG DUP " + std::to_string(previous_trial) + "\n");
		return;
	}

	std::ofstream file(player_file_name, std::ios_base::app); // open file in append mode
	file << "T " << letter << "\n";
	file.close();
	bool word_contains = check_letter_in_string(word, letter[0]);
	if(word_contains){
		if(is_win(player_file_name, word)){
			response = "RLG WIN "+ trial + "\n";
			create_score_file(player_file_name, player_id, word);
			mark_as_done(player_file_name, player_id, "W");
		} else {
			response = "RLG OK " + trial + " " + get_positions_in_word(word, letter) + "\n";
		}
		send_udp_response(response);
		return;
	} else if (!word_contains && (max_errors - get_current_errors_or_success(player_file_name, "error")) == 0) {
		response = "RLG OVR " + trial + "\n";
		mark_as_done(player_file_name, player_id, "L");
		send_udp_response(response);
	} else {
		response = "RLG NOK " + trial + "\n";
		send_udp_response(response);
	}
}

void handle_guess(std::vector <std::string> arguments){
	std::string player_id = arguments[1];
    std::string word = arguments[2];
    std::string trial = arguments[3];
    std::string player_file_name = "GAME_" + player_id + ".txt";
	std::string current_word = get_current_word(player_file_name);
	int current_trial = get_current_trial(player_file_name);
	int max_errors = calculate_max_errors(current_word.size());
	std::string response;
	
	if(arguments.size() != 4 || player_id.size() != 6 || !is_number(player_id) || !is_number(trial)){
        send_udp_response("RWG ERR\n");
        return;
    }

	if(!has_ongoing_game(player_id)){ 
		send_udp_response("RWG ERR\n");
		return;
	}

	if(has_played_before(player_id, word) && trial == std::to_string(current_trial-1)){
		// SEND RESPONSE AGAIN: no idea how ;-;
	}

	if(current_trial != stoi(trial)){
		send_udp_response("RWG INV " + std::to_string(current_trial-1) + "\n");
		return;
	}

	if(has_played_before(player_id, word)){
		int previous_trial = stoi(trial) - 1;
		send_udp_response("RWG DUP " + std::to_string(previous_trial) + "\n");
		return;
	}

	std::ofstream file(player_file_name, std::ios_base::app); // open file in append mode
	file << "G " << word << "\n";
	file.close();
	bool word_contains = check_word_equal(current_word, word);
	if(word_contains){
		// i don't actually need to check this now that i think about it lmao
		response = "RWG WIN " + trial + "\n";
		create_score_file(player_file_name, player_id, word);
		mark_as_done(player_file_name, player_id, "W");
		send_udp_response(response);
		return;
	} else if (!word_contains && (max_errors - get_current_errors_or_success(player_file_name, "error")) == 0) {
		response = "RWG OVR " + trial + "\n";
		mark_as_done(player_file_name, player_id, "L");
		send_udp_response(response);
	} else {
		response = "RWG NOK " + trial + "\n";
		send_udp_response(response);
	}

}

typedef struct {
	int n_scores;
	int score[10];
	std::string PLID[10];
	std::string word[10];
	int n_succ[10];
	int n_tot[10];

} SCORELIST;

int find_top_scores(SCORELIST *list) {
    struct dirent **filelist;
    int n_entries, i_file;
    char fname[50];
    FILE *fp;
	char PLID[6];
	char word[30]; // words are maximum 30 characters
	std::string new_s = "";

    n_entries = scandir("SCORES/", &filelist, 0, alphasort);

    i_file = 0;
    if(n_entries < 0) {
        return (0);
    } else {
        while(n_entries--) {
            if(filelist[n_entries]->d_name[0] != '.') {
                sprintf(fname, "SCORES/%s", filelist[n_entries]->d_name);
                fp = fopen(fname, "r");
                if(fp != NULL) {
                    fscanf(fp, "%d %s %s %d %d",
                        &list->score[i_file], PLID, word,
                        &list->n_succ[i_file], &list->n_tot[i_file]);
						list->PLID[i_file] = new_s + PLID;
						list->word[i_file] = new_s + word;
                    fclose(fp);
                    ++i_file;
                }
            }

            free(filelist[n_entries]);
            if(i_file == 10)
                break;
        }
        free(filelist);
    }
    list->n_scores = i_file;
    return (i_file);
}

void create_scoreboard_file(SCORELIST *scorelist){
	int counter = 1;
	char buffer[100];
	find_top_scores(scorelist);

	std::ofstream file("TOPSCORES.txt");
	file << "-------------------------------- TOP 10 SCORES --------------------------------\n\n";
	file << "   SCORE  PLAYER     WORD                  GOOD TRIALS  TOTAL TRIALS\n\n";
	for(int i = 0; i < scorelist->n_scores; i++){
		sprintf(buffer, " %d - %d\t%s\t%s     %d        %d\n", counter, 
		scorelist->score[i], scorelist->PLID[i].c_str(), scorelist->word[i].c_str(), scorelist->n_succ[i], scorelist->n_tot[i]);
		file << buffer;
		counter++;
	}
	file.close();
}

void write_tcp(int fd, std::string msg) {
    int n;
    int size = msg.size();

    while(size > 0 && (n = write(fd, msg.c_str(), size)) > 0) {    
        msg.erase(0, n);
        size = msg.size();

        std::cout << "write_tcp n: " << n << "\n";
    }
    if(n == -1) {
        std::cout << "error in read()\n in function handle_tcp_request()\nERROR: " << strerror(errno) << "\n";
        close(fd);
        exit(1);
    }

}

void handle_scoreboard(int fd){
	std::string cmd, line;
	SCORELIST scores;
	create_scoreboard_file(&scores);

	print_if_verbose("GSB");

    std::ifstream file("TOPSCORES.txt", std::ios::binary);
    file.seekg(0, std::ios::end);
    int file_size = file.tellg();

    cmd = "RSB OK scoreboard.txt " + std::to_string(file_size) + " ";
    write_tcp(fd, cmd);

    file.seekg(0, std::ios::beg);
    while(getline(file, line)) {
        write_tcp(fd, line.append("\n"));
    }

    write_tcp(fd, "\n");
    close(fd);
    exit(0);
}

void read_tcp(int fd, int size, char* buffer) {
    int n, offset = 0;
    std::string buffer_str;

    while(size > 0 && (n = read(fd, buffer + offset, size)) > 0) {
        size -= n;
        offset += n;
    }
    if(n == -1) {
        std::cout << "error in read()\n in function handle_tcp_request()\nERROR: " << strerror(errno) << "\n";
        close(fd);
        exit(1);
    }
}

void handle_hint(int fd){
    char buffer[512];
    std::string pl_id = "", hint, path, cmd, buffer_str;
    int n, offset = 0;

    read_tcp(fd, 7, buffer);
    pl_id.append(buffer, 0, 6);

    path  = "GAME_" + pl_id + ".txt";
	print_if_verbose("GHL " + pl_id);

    std::ifstream statefile(path, std::ios::binary);
    if(!statefile.good()) {
        write_tcp(fd, "RHL NOK\n");
        close(fd);
        exit(1);
    }

    getline(statefile, hint, ' ');
    getline(statefile, hint);

    std::ifstream hintfile(hint);
    if(!hintfile.good()) {
        write_tcp(fd, "RHL NOK\n");
        close(fd);
        exit(1);
    }

    hintfile.seekg(0, hintfile.end);
    int file_size = hintfile.tellg();

    cmd = "RHL OK " + hint + " " + std::to_string(file_size) + " ";
    write_tcp(fd, cmd);

    hintfile.seekg(0, hintfile.beg);



    if(hintfile.is_open()) {
        while(file_size > 0) {
            hintfile.seekg(offset, hintfile.beg);
            hintfile.read(buffer, 512);
            n = write(fd, buffer, 512);
            if(n == -1) {
                std::cout << "error in write()\n in function handle_tcp_request()\nERROR: " << strerror(errno) << "\n";
                close(fd);
                exit(1);
            }
            offset += n;
            file_size -= n;
        }
    }

    write_tcp(fd, "\n");
    close(fd);

    exit(0);
}	

int isDirectoryEmpty(const char *dirname) {
	int n = 0;
	struct dirent *d;
	DIR *dir = opendir(dirname);
	if (dir == NULL)
		return 1;
	while ((d = readdir(dir)) != NULL) {
		if(++n > 2)
			break;
		}
	closedir(dir);
	if (n <= 2) 
		return 1;
	else
		return 0;
}

std::vector<std::string> grab_file_names(std::string path){
	std::vector<std::string> result;
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir (path.c_str())) != NULL) {
  	/* print all the files and directories within directory */
  		while ((ent = readdir (dir)) != NULL) {
			result.push_back(ent->d_name);
  		}
  		closedir (dir);
	} else {
  		/* could not open directory */
  		perror ("");
	}
	return result;
}

std::string get_latest_file_name(std::string path){
	std::vector<std::string> files = grab_file_names(path);
	std::sort(files.begin(), files.end());

	return files[files.size()];
}

void handle_state(int fd){
	std::string player_id;
	char buffer[50];

	read_tcp(fd, 7, buffer);
	player_id.append(buffer, 0, 6);
	print_if_verbose("STA " + player_id);
	std::string filename = "GAME_" + player_id + ".txt";
	std::string line;

	if(player_id.size() != 6 || !is_number(player_id)){
		std::string cmd = "RST NOK\n";
    	write_tcp(fd, cmd);
		return;
	}

	if(has_ongoing_game(player_id)){
		std::string cmd;
		std::ifstream file(filename, std::ios::binary);
    	file.seekg(0, std::ios::end);
    	int file_size = file.tellg();

    	cmd = "RST OK STATE_" + player_id + ".txt " + std::to_string(file_size) + " ";
    	write_tcp(fd, cmd);

    	file.seekg(0, std::ios::beg);
    	while(getline(file, line)) {
        	write_tcp(fd, line.append("\n"));
    	}

		write_tcp(fd, "\n");
    	close(fd);
	} else {
		struct stat info;
		std::string path = "GAMES/" + player_id + "/";
		stat(path.c_str(), &info);
		if(info.st_mode & S_IFDIR) {
			if(isDirectoryEmpty(path.c_str())){
				std::string cmd = "RST NOK\n";
    			write_tcp(fd, cmd);
				return;
			}
			std::string name = get_latest_file_name(path);
			std::string pathhh = "GAMES/" + player_id + "/" + name;
			std::string cmd;
			std::ifstream file(pathhh, std::ios::binary);
    		file.seekg(0, std::ios::end);
    		int file_size = file.tellg();

    		cmd = "RST OK STATE_" + player_id + ".txt " + std::to_string(file_size) + " ";
    		write_tcp(fd, cmd);

    		file.seekg(0, std::ios::beg);
    		while(getline(file, line)) {
        		write_tcp(fd, line.append("\n"));
    		}

			write_tcp(fd, "\n");
    		close(fd);
		} else {
    		std::string cmd = "RST NOK\n";
    		write_tcp(fd, cmd);
			return;
		}
	}

}

void handle_quit(std::vector <std::string> arguments){
	std::string player_id = arguments[1];
	std::string filename = "GAME_" + player_id + ".txt";

	if(arguments.size() != 2 || player_id.size() != 6 || !is_number(player_id)){
		send_udp_response("RQT ERR\n");
		return;
	}

	if(has_ongoing_game(player_id)){
		mark_as_done(filename, player_id, "Q");
		send_udp_response("RQT OK\n");
	} else {
		send_udp_response("RQT NOK\n");
	}
}

std::vector <std::string> get_arguments(char* buffer){
	std::string request(buffer);
    std::stringstream ss(request);
    std::string token;
    std::vector <std::string> arguments;

    while (ss >> token) {
        arguments.push_back(token);
    }

	return arguments;
}

void handle_tcp_connect(int newfd){
    char buffer[512];
    int n, offset = 0, size = TCP_CMD_CHAR;
    std::string cmd;

    read_tcp(newfd, 4, buffer);
    cmd.append(buffer, 0, 3);



    if(cmd == "GSB") {
        handle_scoreboard(newfd);
    } else if(cmd == "GHL") {
        handle_hint(newfd);
    } else if(cmd == "STA"){
		handle_state(newfd);
	}

    freeaddrinfo(tcp_result);
    if(close(server_tcp_socket_fd) == -1) {
        std::cout << "error in close()\n in function\nERROR: " << strerror(errno) << "\n";
        exit(1);
    }    
    exit(0);
}

void initiate_parent_tcp_thread(){
    int newfd, n, nfork;
    struct sockaddr_in address, addrh;

    bool is_running = true;
    while(is_running){
        socklen_t addr_length = sizeof(addrh);

        if((newfd = accept(server_tcp_socket_fd, (struct sockaddr*)&address, &addr_length)) == -1){ // mind when quit or exit are used
            std::cout << "error: accept line 165 \n" ; exit(1);
        }

        nfork = fork();
        if(nfork == 0){
            handle_tcp_connect(newfd);
        } else {
            continue;
        }
    }
}

void handle_udp_requests(){
	int n;
	socklen_t addrlen = sizeof(udp_addr);
	char buffer[128];

	std::vector <std::string> arguments;

	while(1){
		n = recvfrom(server_udp_socket_fd, buffer, 128, 0, (struct sockaddr*) &udp_addr, &addrlen);	
		if(n == -1 || buffer[n-1] != '\n') { std::cout << "error: recvfrom line 216\n"; return; }

		buffer[n-1] = '\0';
		print_if_verbose(buffer);
		get_arguments(buffer).swap(arguments);

		if(arguments[0] == "SNG"){
			handle_start(arguments);
		} else if (arguments[0] == "PLG"){
			handle_play(arguments);
		} else if (arguments[0] == "PWG"){
			handle_guess(arguments);
		} else if (arguments[0] == "QUT"){
			handle_quit(arguments);
		}
	}
}

int main(int argc, char const **argv){
    int n;

    if(!handle_server_input(argc, argv)){
        std::cout << WRONG_INPUT_ERROR_MSG;
        exit(1);
    }

    populate_words_list(word_file);

    setup_sockets();

    n = fork();
    if(n == 0){
        initiate_parent_tcp_thread();
    } else {
        handle_udp_requests();
    }

    return 0;
}