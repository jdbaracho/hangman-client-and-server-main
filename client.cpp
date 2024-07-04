#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <fstream>
#include <sys/time.h>
#include <errno.h>

#define DEFAULT_GS_IP "localhost"
#define DEFAULT_GS_PORT "58019" // 58000 + Group Number (19)

#define WRONG_INPUT_ERROR_MSG "Wrong Input. Usage: ./player [-n GSIP] [-p GSport]\n"

#define WRONG_START_CMD "Wrong Input. Usage: start/sg PLID\n"
#define START_MSG(l, e) ("New game started.\nGuess the " + l + " letter word with less than " + e + " errors: ")

#define WRONG_PLAY_CMD "Wrong Input. Usage: play/pl letter\n"

#define WRONG_GUESS_CMD "Wrong Input. Usage: guess/gw word\n"

#define HEADER_CHARS 44 // CMD(3B) + status(max 3B) + Fname(max 24B) + Fsize(max 10B) + spaces(4B) = 44

std::string gs_ip = DEFAULT_GS_IP;
std::string gs_port = DEFAULT_GS_PORT;
std::string pl_id;
std::string current_word;

int trials = 0;

int client_udp_socket_fd;
int client_tcp_socket_fd;

struct addrinfo *udp_result, *tcp_result;
struct sockaddr_in udp_addr, tcp_addr;
struct timeval timeout;


// true if string is a number
bool is_number(std::string s) {
	for(char c: s) {
		if(!isdigit(c)) { return false; }
	}
	return true;
}

// returns true if successful, false if an error ocurred
bool handle_client_input(int argc, const char **argv) {

	switch (argc) {

		// both default
		case 1:
			return true;

		// one default, one input
		case 3:
			if(!strcmp(argv[1], "-n")) { gs_ip = argv[2]; return true; }
			if(!strcmp(argv[1], "-p")) { gs_port = argv[2]; return true; }
			return false;

		// both inputs
		case 5:
			if(!strcmp(argv[1], "-n") && !strcmp(argv[3], "-p")) {
				gs_ip = argv[2];
				gs_port = argv[4];
				return true;
			}
			
			if(!strcmp(argv[1], "-p") && !strcmp(argv[3], "-n")) {
				gs_ip = argv[4];
				gs_port = argv[2];
				return true;
			} 
			
			return false;
		
		default:
			return false;
		};
}

// read input and split by space
std::vector<std::string> get_command(std::string input) {
	std::vector<std::string> cmd;
	std::string word;

	std::stringstream ss(input);

	while(getline(ss, word, ' ')) { cmd.push_back(word); }

	return cmd;
}

void initialize_sockets() {
	int n, err;
	struct addrinfo udp_hints;

	// UDP socket
	client_udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(client_udp_socket_fd == -1) {
		std::cout << "error in socket()\n in function initialize_sockets()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}
	
	memset(&udp_hints, 0, sizeof udp_hints);
	udp_hints.ai_family = AF_INET;
	udp_hints.ai_socktype = SOCK_DGRAM;

	err = getaddrinfo(gs_ip.c_str(), gs_port.c_str(), &udp_hints, &udp_result);
	if(err != 0) {
		std::cout << "error in getaddrinfo()\n in function initialize_sockets()\nERROR: ";
		if(err == EAI_SYSTEM) { std::cout << strerror(errno) << "\n"; }
		else { std::cout << gai_strerror(errno) << "\n"; } 
		exit(1);
	}
}

void close_sockets() {
	freeaddrinfo(udp_result);
	if(close(client_udp_socket_fd) == -1) {
		std::cout << "error in close()\n in function close_sockets()\nERROR: " << strerror(errno) << "\n";
		exit(1); 
	};
}

void establish_tcp() {
	int n, err;
	struct addrinfo tcp_hints;
	
	// TCP socket
	client_tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_tcp_socket_fd == -1) {
		std::cout << "error in socket()\n in function establish_tcp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}

	memset(&tcp_hints, 0, sizeof tcp_hints);
	tcp_hints.ai_family = AF_INET;
	tcp_hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(gs_ip.c_str(), gs_port.c_str(), &tcp_hints, &tcp_result);
	if(err != 0) {
		std::cout << "error in getaddrinfo()\n in function establish_tcp()\nERROR: ";
		if(err == EAI_SYSTEM) { std::cout << strerror(errno) << "\n"; }
		else { std::cout << gai_strerror(errno) << "\n"; } 
		exit(1);
	}
	
	n = connect(client_tcp_socket_fd, tcp_result->ai_addr, tcp_result->ai_addrlen);
	if(n == -1) {
		std::cout << "error in connect()\n in function establish_tcp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}
}

inline void send_command_udp(std::string command) {
	int n;

	timeout.tv_sec = 1200; // set to 10 before submit
	timeout.tv_usec = 0;
	if(setsockopt(client_udp_socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
		std::cout << "error in setsockopt()\n in function send_command_udp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}

	n = sendto(client_udp_socket_fd, command.c_str(), strlen(command.c_str()), 0, udp_result->ai_addr, udp_result->ai_addrlen);
	if(n == -1) {
		std::cout << "error in sendto()\nin function send_command_udp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}
}

std::string await_response_udp() {
	int n;
	socklen_t addrlen = sizeof(udp_addr);
	char buffer[128];
	std::string buffer_str;

	timeout.tv_sec = 5; // set to 10 before submit
	timeout.tv_usec = 0;
	if(setsockopt(client_udp_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
		std::cout << "error in setsockopt()\n in function await_response_udp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}	

	n = recvfrom(client_udp_socket_fd, buffer, 128, 0, (struct sockaddr*) &udp_addr, &addrlen);
	if(n == -1) {
		std::cout << "error in recvfrom()\nin function await_response_udp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}

	if(buffer[n-1] != '\n') { return "CORRUPTED"; }

	write(1, buffer, n);

	buffer_str.append(buffer, 0, n-1);

	return buffer_str;
}

int read_tcp(int fd, int size, char* buffer) {
    int n, offset = 0;
	std::string buffer_str;

	while(size > 0 && (n = read(fd, buffer + offset, size)) > 0) {
        size -= n;
		offset += n;
	}
    if(n == -1) {
		std::cout << "error in read()\n in function read_tcp()\nERROR: " << strerror(errno) << "\n";
		exit(1);
	}
	return offset;
}

std::vector<std::string> handle_tcp_request(std::string command, std::string request, std::string answer, std::string no_file) {
    int n, offset = 0, msg_size = 0, size, fsize;
    char buffer[1024];
    std::vector<std::string> response;
    std::string buffer_str, word, tag, status, fname, fsize_str;

	size = command.size();

    while(size > 0) {
        n = write(client_tcp_socket_fd, command.c_str(), strlen(command.c_str()));
        if(n == -1) {
			response.clear();
			response.push_back("errno");
			response.push_back("error in write()\n in function handle_tcp_request()\nERROR: ");
			return response; }

        command.erase(0, n);
        size = command.size();

    }

	n = read_tcp(client_tcp_socket_fd, 4, buffer);
	tag.append(buffer, 0, 3);
	response.push_back(tag);
	

	if(tag != answer) {
		response.clear();
		response.push_back("err");
		response.push_back("corrupted response\nin function handle_scoreboard()\n");
		return response;
	}

	do {
		read_tcp(client_tcp_socket_fd, 1, buffer + offset);
		offset++;
		
	} while(buffer[offset-1] != ' ' && buffer[offset-1] != '\n');

	status.append(buffer, 0, offset-1);
	response.push_back(status);

	if(status == no_file) { return response; }

	offset = 0;
	do {
		read_tcp(client_tcp_socket_fd, 1, buffer + offset);
		offset++;
		
	} while(buffer[offset-1] != ' ');

	fname.append(buffer, 0, offset-1);
	response.push_back(fname);

	offset = 0;
	do {
		read_tcp(client_tcp_socket_fd, 1, buffer + offset);
		offset++;
		
	} while(buffer[offset-1] != ' ');

	fsize_str.append(buffer, 0, offset-1);
	response.push_back(fsize_str);

    std::ofstream file(response[2], std::ios::out | std::ios::binary);

	fsize = atoi(response[3].c_str());

	while(fsize > 0) {
		if(fsize > 1024) { size = 1024; } else { size = fsize; }
		n = read_tcp(client_tcp_socket_fd, size, buffer);
		fsize -= n;
		file.write(buffer, n*sizeof(char));
		if(request == "GSB" || request == "STA") { write(1, buffer, n); }
	}

    file.close();
    return response;
}

std::string operator * (std::string a, unsigned int b) {
    std::string output = "";
    while (b--) {
        output += a;
    }
    return output;
}

inline int find_letter_index(int number) {
	switch(number){
		case 1:
			return 0;
		case 2: 
			return 2;
		default:
			return (2*number - 2);
	}
}

int handle_start(std::vector<std::string> command) {
	std::string cmd, response;
	std::vector <std::string> arguments;

	if(command.size() != 2) { std::cout << WRONG_START_CMD; return 1; }
	
	cmd = "SNG " + command[1] + "\n";
	pl_id = command[1];

	send_command_udp(cmd);
	response = await_response_udp();
	if(response == "CORRUPTED") { std::cout << "Corrupted response\nin function handle_start()\n"; return 1;}
	if(response == "ERR") { std::cout << "Invalid player ID\n"; return 1;}

	get_command(response).swap(arguments);

	switch(arguments.size()) {
	case 2:
		if(!(arguments[0] == "RSG" && arguments[1] == "NOK")) {
			std::cout << "Corrupted NOK response\nin function handle_start()";
			return 1;
		}
		std::cout << "You have an ongoing game.\n";
		return 0;
		break;

	case 4:
		if(!(arguments[0] == "RSG" && arguments[1] == "OK" &&
		is_number(arguments[2]) && is_number(arguments[3]))) {
			std::cout << "Corrupted OK response\nin function handle_start()\n";
			return 1;
		}
		break;
	
	default:
		std::cout << "Corrupted response\nin function handle_start()\n"; return 1;
		break;
	}

	std::cout << START_MSG(arguments[2], arguments[3]);

	int n_letters = atoi(arguments[2].c_str());

	std::string word_symbol = "_ ";
	current_word = (word_symbol * n_letters);
	std::cout << current_word << "\n";

	return 0;
}

int handle_play(std::vector<std::string> command) {
	std::string cmd, response;
	std::vector <std::string> arguments;

	if(command.size() != 2 || command[1].size() != 1 || !isalpha(command[1].c_str()[0])) {
		std::cout << WRONG_PLAY_CMD;
		return 1;
	}

	std::string trials_number = std::to_string(trials + 1);
	char letter = std::toupper(command[1][0]);
	
	cmd = "PLG " + pl_id + " " + letter + " " + trials_number + "\n";

	send_command_udp(cmd);
	response = await_response_udp();
	if(response == "CORRUPTED") { std::cout << "Corrupted response\nin function handle_play()\n"; return 1;}

	get_command(response).swap(arguments);
	if(arguments[0] != "RLG" || arguments.size() < 2) {
		std::cout << "Corrupted response\nin function handle_play()\n";
		return 1;
	}


	if(arguments[1] == "OK"){
		int n = atoi(arguments[3].c_str());

		if(arguments.size() != 4 + n) { std::cout << "Corrupted response\nin function handle_play()\n"; return 1; }

		for(int i = 4; i < n+4; i++) {
			current_word[find_letter_index(atoi(arguments[i].c_str()))] = letter;
		}

		std::cout << "The word contains the letter " << letter << "!\n";
	} else if(arguments[1] == "WIN"){

		for(int i = 0; i < current_word.size(); i++){
			if(current_word[i] == '_'){
				current_word[i] = letter;
			}
		}
		trials = 0;
		std::cout << "You have won. The word was ";
		if(current_word.size() != 0) { std::cout << current_word << '\n'; }
        return 0;
	} else if(arguments[1] == "DUP"){
		std::cout << "You have already played that letter.\n";
		trials -= 1;
	} else if(arguments[1] == "NOK"){
		std::cout << "The word does not contain the letter " << letter << ".\n";
	} else if(arguments[1] == "OVR"){
		std::cout << "The word does not contain the letter " << letter << " and you have ran out of tries. Ending the game.\n";
	} else if(arguments[1] == "INV"){
		std::cout << "Trial number not valid or already used.\n";
		trials = atoi(arguments[2].c_str());
	} else if(arguments[1] == "ERR"){
		std::cout << "Error occured. PLID is invalid or no ongoing game exists for the given PLID.\n";
	} else {
		std::cout << "Corrupted response\nin function handle_play()\n"; return 1;
	}

	if(current_word.size() != 0) { std::cout << current_word << '\n'; }

	trials += 1;

	return 0;
}

int handle_guess(std::vector<std::string> command) {
	std::string cmd, response;
	std::vector <std::string> arguments;

	if(command.size() != 2) {std::cout << WRONG_GUESS_CMD; return 1; }
	for(char c: command[1]) {
		if(!isalpha(c)) { std::cout << WRONG_GUESS_CMD; return 1;  }
	}

	std::string trials_number = std::to_string(trials + 1);

	cmd = "PWG " + pl_id + " " + command[1] + " " + trials_number + "\n";
	
	send_command_udp(cmd);
	response = await_response_udp();
	if(response == "CORRUPTED") { std::cout << "Corrupted response\nin function handle_guess()\n"; return 1; }

	get_command(response).swap(arguments);

	if(arguments[0] != "RWG") { std::cout << "Corrupted response\nin function handle_guess()\n"; return 1; }

	if(arguments[1] == "WIN"){
		trials = 0;
		std::cout << "You have won. The word was " << command[1] << ".\n";
		return 0;
	} else if(arguments[1] == "DUP"){
		std::cout << "You have already tried to guess with that word.\n";
		trials -= 1;
	} else if(arguments[1] == "NOK"){
		std::cout << "The word is not " << command[1] << ".\n";
	} else if(arguments[1] == "OVR"){
		std::cout << "The word is not " << command[1] << " and you have ran out of tries. Ending the game.\n";
	} else if(arguments[1] == "INV"){
		std::cout << "Trial number not valid or already used.\n";
	} else if(arguments[1] == "ERR"){
		std::cout << "Error occured. PLID is invalid or no ongoing game exists for the given PLID.\n";
	} else {
		std::cout << "Corrupted response\nin function handle_guess()\n"; return 1;
	}

	trials += 1;

	return 0;
}

int handle_scoreboard() {
	std::string cmd;
	std::vector<std::string> response;

	cmd = "GSB\n";

	// open tcp
	establish_tcp();
	// make request
	response = handle_tcp_request(cmd, "GSB", "RSB", "EMPTY");
	if(response[0] == "errno") { std::cout << response[1] << strerror(errno) << "\n"; exit(1); }
	// close tcp
	freeaddrinfo(tcp_result);
	if(close(client_tcp_socket_fd) == -1) {
		std::cout << "error in close()\n in function handle_scoreboard()\nERROR: " << strerror(errno) << "\n";
		exit(1); 
	}

	if(response[0] == "err") { std::cout << response[1]; return 1; }

	switch(response.size()){
		case 2:
			std::cout << "The scoreboard is empty\n";
			break;

		case 4:
			std::cout << "file name: " << response[2] << "\nsize: " << response[3] << "\n";
			break;

		default:
			std::cout << "error in function handle_scoreboard()\n";
			return 1;
	}

	return 0;
}

int handle_hint() {
	std::string cmd;
	std::vector<std::string> response;

	cmd = "GHL " + pl_id + "\n";

	// open tcp
	establish_tcp();
	// make request
	response = handle_tcp_request(cmd, "GHL", "RHL", "NOK");
	if(response[0] == "errno") { std::cout << response[1] << strerror(errno) << "\n"; exit(1); }
	// close tcp
	freeaddrinfo(tcp_result);
	if(close(client_tcp_socket_fd) == -1) {
		std::cout << "error in close()\n in function handle_hint()\nERROR: " << strerror(errno) << "\n";
		exit(1); 
	}

	if(response[0] == "err") { std::cout << response[1]; return 1; }

	switch(response.size()){
		case 2:
			std::cout << "There is no file for this word.\n";
			break;

		case 4:
			std::cout << "file name: " << response[2] << "\nsize: " << response[3] << "\n";
			break;

		default:
			std::cout << "error in function handle_hint()\n";
			return 1;
	}

	return 0;
}

int handle_state() {
	std::string cmd;
	std::vector<std::string> response;

	cmd = "STA " + pl_id + "\n";

	// open tcp
	establish_tcp();
	// make request
	response = handle_tcp_request(cmd, "STA", "RST", "NOK");
	if(response[0] == "errno") { std::cout << response[1] << strerror(errno) << "\n"; exit(1); }
	// close tcp
	freeaddrinfo(tcp_result);
	if(close(client_tcp_socket_fd) == -1) {
		std::cout << "error in close()\n in function handle_state()\nERROR: " << strerror(errno) << "\n";
		exit(1); 
	}

	if(response[0] == "err") { std::cout << response[1]; return 1; }

	switch(response.size()){
		case 2:
			std::cout << "There are no games for this player.\n";
			break;

		case 4:
			std::cout << "file name: " << response[2] << "\nsize: " << response[3] << "\n";
			break;

		default:
			std::cout << "error in function handle_state()\n";
			return 1;
	}

	if(response[1] == "FIN") { std::cout << "This game has ended."; }

	return 0;
}

int handle_quit() {
	std::string cmd = "QUT " + pl_id + "\n";

	// these are just for debugging, not gonna be in the final client
	std::string cmd2 = "KILLGAME " + pl_id + "\n";
	std::string cmd3 = "KILLPDIR " + pl_id + "\n";

	send_command_udp(cmd);
	send_command_udp(cmd2);
	send_command_udp(cmd3);
	// we dont need to wait for the response to cmd2 and cmd3 because the server doesn't respond to those requests
	await_response_udp(); // we should add a timeout here, so the client doesn't wait forever in case the response gets lost

	return 0;
}

int handle_exit() {
	std::string cmd = "QUT " + pl_id + "\n";

	// these are just for debugging, not gonna be in the final client
	std::string cmd2 = "KILLGAME " + pl_id + "\n";
	std::string cmd3 = "KILLPDIR " + pl_id + "\n";

	send_command_udp(cmd);
	send_command_udp(cmd2);
	send_command_udp(cmd3);
	// we dont need to wait for the response to cmd2 and cmd3 because the server doesn't respond to those requests
	await_response_udp(); // we should add a timeout here, so the client doesn't wait forever in case the response gets lost

	return 0;
}

//-------------------------------------------------
void request_word(){
    std::string command = "REV " + pl_id + "\n";
    send_command_udp(command);
    await_response_udp();
}
//-------------------------------------------------

int main(int argc, const char **argv) {
	if(handle_client_input(argc, argv)){
		initialize_sockets();

		std::string input;
		std::vector<std::string> cmd;

		// wait for command
		while(1) {
			getline(std::cin, input);
			get_command(input).swap(cmd);

			if(cmd[0] == "start" || cmd[0] == "sg") {
				if(handle_start(cmd)) { continue; }
			} else if(cmd[0] == "play" || cmd[0] == "pl") {
				if(handle_play(cmd)) { continue; }
			} else if(cmd[0] == "guess" || cmd[0] == "gw") {
				if(handle_guess(cmd)) { continue; }	
			} else if(cmd[0] == "scoreboard" || cmd[0] == "sb") {
				if(handle_scoreboard()) { continue; }	
			} else if(cmd[0] == "hint" || cmd[0] == "h") {
				if(handle_hint()) { continue; }	
			} else if(cmd[0] == "state" || cmd[0] == "st") {
				if(handle_state()) { continue; }	
			} else if(cmd[0] == "quit") {
				if(handle_quit()) { continue; }
			} else if(cmd[0] == "exit") {
				if(handle_exit()) { continue; }
				break;
			} else if(cmd[0] == "wordpls") {
                request_word();
                continue;
			}
		}

		close_sockets();

	} else {
		std::cout << WRONG_INPUT_ERROR_MSG;
		exit(1);
	}

	return 0;
}