#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define BUFFER_SIZE 100
#define MAX_TOKENS 10
#define TRUE 1
// Öğrenci yapısı
typedef struct {
    char name[50];
    char grade[3];
} Student;

char* getCurrentTime() {
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char *timeString = asctime(timeinfo);
    size_t len = strlen(timeString);
    if (len > 0 && timeString[len - 1] == '\n') {
        timeString[len - 1] = '\0';
    }
    return timeString;
}

// Function to write log entry to file
void writeToLog(char *logMessage) {
    int fd = open("logfile.txt", O_WRONLY | O_APPEND | O_CREAT, 0644); // Open or create log file in append mode
    if (fd == -1) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    
    char *currentTime = getCurrentTime();
    if (currentTime != NULL) {
        int timeLength = strlen(currentTime);
        if (write(fd, currentTime, timeLength) != timeLength) {
            perror("Error writing time stamp to log file");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    
    if (write(fd, " ", 1) != 1) {
        perror("Error writing space to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    int messageLength = strlen(logMessage);
    if (write(fd, logMessage, messageLength) != messageLength) {
        perror("Error writing log message to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    if (write(fd, "\n", 1) != 1) {
        perror("Error writing newline to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}
int cmdSize(const char * command)
{
	int i = 0;

	while (command[i] != '\0')
	{
		i++;
	}
	return i;
}
void trim(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace(str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
}

// Function to add student grade to file
void addStudentGrade(char **tokensNameSurname, int tokenCount, char *grade, char *fileName) {
    //if the file exists
    if (access(fileName, F_OK) != -1) {
        // open file
        int fd = open(fileName, O_RDWR | O_APPEND, 0666);
        if (fd == -1) {
            perror("Opening file failed\n");
            return;
        }
        // write to file
        for (int i = 0; i < tokenCount; i++) {
            printf("%s ", tokensNameSurname[i]);
            write(fd, tokensNameSurname[i], strlen(tokensNameSurname[i])); // write name and surname to file
            if (i < tokenCount - 1) {
                write(fd, " ", 1); // add space between name and surname
            }
        }
        printf(", %s added to %s\n", grade, fileName);
        write(fd, ",", 1); // after name and surname, add comma
        write(fd, grade, strlen(grade)); // write grade to file
        write(fd, "\n", 1); // add newline character
        
        // close file
        close(fd);
        
        pid_t pid=fork();
        if(pid == -1){
            printf("Fork failed\n");
        }
        else if(pid == 0){
            // Example of logging a command execution
            char logMessage[BUFFER_SIZE];
            snprintf(logMessage, BUFFER_SIZE, "Done Succesfully\n");
            writeToLog(logMessage);
            exit(EXIT_SUCCESS);
        }
        else{
            // Ana işlem
            int status;
            wait(&status);
        }
    } else {
        // Dosya yok, hata mesajı göster
        char message[]="Error: File not found\n";
        write(STDOUT_FILENO, message, cmdSize(message));
        
        pid_t pid=fork();
        if(pid == -1){
            printf("Fork failed\n");
        }
        else if(pid == 0){
            // Example of logging a command execution
            char logMessage[BUFFER_SIZE];
            snprintf(logMessage, BUFFER_SIZE, "Error: File not found\n");
            writeToLog(logMessage);
            exit(EXIT_SUCCESS);
        }
        else{
            // Ana işlem
            int status;
            wait(&status);
        }
    }
}

#define MAX_LINE_LENGTH 1000

void searchStudent(const char *nameSurname, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Dosya açılamadı");
        return;
    }

    char line[MAX_LINE_LENGTH];
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        char *token = strtok(line, ",");
        if (token != NULL && strcmp(token, nameSurname) == 0) {
            printf("%s isimli kişi, %s dosyasının %d. satırında bulundu. Grade: ", nameSurname, filename, lineNumber);
            char logMessage[BUFFER_SIZE];
            snprintf(logMessage, BUFFER_SIZE, "%s person be found in %s filename in %d line\n", nameSurname, filename, lineNumber);
            writeToLog(logMessage);
            token = strtok(NULL, ",");
            if (token != NULL) {
                printf("%s\n", token);
            } else {
                printf("Bilgi bulunamadı.\n");
            }
            fclose(file);
            return;
        }
    }
    writeToLog("Error: Name not found\n");
    printf("%s isimli kişi, %s dosyasında bulunamadı.\n", nameSurname, filename);
    fclose(file);
}
int ascendingNameCompare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int descendingNameCompare(const void *a, const void *b) {
    return strcmp(*(const char **)b, *(const char **)a);
}



// Verilen dosyadan öğrenci bilgilerini okuyan fonksiyon
int readStudentsFromFile(Student **students, int fd) {
    int bufferSize = 1024;
    char buffer[bufferSize];
    int count = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, bufferSize)) > 0) {
        char *token = strtok(buffer, "\n");
        while (token != NULL) {
            sscanf(token, "%[^,],%s", (*students)[count].name, (*students)[count].grade);
            count++;
            token = strtok(NULL, "\n");
        }
    }
    return count;
}

// Öğrenci bilgilerini terminale yazdıran fonksiyon
void printStudents(Student *students, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s,%s\n", students[i].name, students[i].grade);
    }
}

// Bubble sort kullanarak öğrencileri notlarına göre sıralayan fonksiyon
void sortByGrade(Student *students, int count,int flag) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if(flag==1) {
                if (strcmp(students[j].grade, students[j + 1].grade) > 0) {
                // Swap işlemi
                Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
                }
            }
            else if(flag == 0){
                if (strcmp(students[j].grade, students[j + 1].grade) < 0) {
                // Swap işlemi
                Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
            }
            }
            
        }
    }
}
void sortName(int (*compareFunction)(const void *, const void *),int fd){
    // Satırları sıralamak için bir bağlı liste oluştur
        typedef struct Node {
            char *data;
            struct Node *next;
        } Node;

        Node *head = NULL;
        char buffer[1024]; // Satır uzunluğu için bir tampon
        ssize_t bytesRead;
        while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
            // Satır sonunu işaretle
            buffer[bytesRead] = '\0';
            // Satırları parçala ve bağlı listeye ekle
            char *token = strtok(buffer, "\n");
            while (token != NULL) {
                Node *newNode = malloc(sizeof(Node));
                newNode->data = strdup(token); // Bellekte yeni bir kopya oluştur
                newNode->next = head;
                head = newNode;
                token = strtok(NULL, "\n");
            }
        }
        // Bağlı listedeki elemanları diziye kopyala
        int lineCount = 0;
        Node *current = head;
        while (current != NULL) {
            lineCount++;
            current = current->next;
        }
        char *lines[lineCount];
        current = head;
        for (int i = 0; i < lineCount; i++) {
            lines[i] = current->data;
            current = current->next;
        }
        // Sıralanmış satırları ekrana yaz
        qsort(lines, lineCount, sizeof(char *), compareFunction);
        for (int i = 0; i < lineCount; i++) {
            printf("%s\n", lines[i]);
        }

        // Belleği serbest bırak
        current = head;
        while (current != NULL) {
            Node *temp = current;
            current = current->next;
            free(temp->data);
            free(temp);
        }
}
void sortAll(char *fileName) {
    // Dosyanın var olup olmadığını kontrol et
    if (access(fileName, F_OK) != -1) {
        // Dosya var, aç
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Opening file failed\n");
            return;
        }
        // Kullanıcıya sıralama yöntemini seçme imkanı sun
        printf("Enter sorting method:\n");
        printf("1. Name ascending order\n");
        printf("2. Name descending order\n");
        printf("3. Grade ascending order\n");
        printf("4. Grade descending order\n");
        int choice;
        scanf("%d", &choice);
        //clean the buffer
        while ((getchar()) != '\n');
        int (*compareFunction)(const void *, const void *);
        if (choice == 1){
            compareFunction = ascendingNameCompare;
            sortName(compareFunction,fd);
            writeToLog("Done Successfully Name Ascending Order\n");
        }
        else if (choice == 2){
            compareFunction = descendingNameCompare;
            sortName(compareFunction,fd);
            writeToLog("Done Successfully Name Descending Order\n");
        }
        else if (choice == 3){
            // Dinamik bellek tahsisi ile öğrenci dizisi oluştur
            Student *students = (Student *)malloc(sizeof(Student) * BUFFER_SIZE);
            if (students == NULL) {
                perror("Bellek tahsisi yapılamadı");
                close(fd);
                return ;
            }

            int count = readStudentsFromFile(&students, fd);
            if (count == -1) {
                free(students);
                close(fd);
                return ;
            }
            sortByGrade(students, count,1);

            printf("Grade Ascending Order\n");
            printStudents(students, count);
            writeToLog("Done Successfully Grade Ascending Order\n");
            // Dinamik bellek alanını serbest bırak
            free(students);   
        }
        else if (choice == 4){
            // Dinamik bellek tahsisi ile öğrenci dizisi oluştur
            Student *students = (Student *)malloc(sizeof(Student) * BUFFER_SIZE);
            if (students == NULL) {
                perror("Bellek tahsisi yapılamadı");
                close(fd);
                return ;
            }

            int count = readStudentsFromFile(&students, fd);
            if (count == -1) {
                free(students);
                close(fd);
                return ;
            }

            
            sortByGrade(students, count,0);

            printf("Grade Descending Order\n");
            printStudents(students, count);
            writeToLog("Done Successfully Grade Descending Order\n");
            // Dinamik bellek alanını serbest bırak
            free(students); 
        }
        else {
            writeToLog("Invalid choice\n");
            printf("Invalid choice\n");
            return;
        }

        
        close(fd);

        

        
        
    } else {
        writeToLog("Error: File not found\n");
        char message[] = "Error: File not found\n";
        write(STDOUT_FILENO, message, cmdSize(message));
    }
}

void bubbleSort(char *arr[], int n, int (*compareFunction)(const void *, const void *)) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (compareFunction(&arr[j], &arr[j + 1]) > 0) {
                // İki elemanın yerini değiştir
                char *temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void showAll(char *fileName) {
    // Dosyanın var olup olmadığını kontrol et
    if (access(fileName, F_OK) != -1) {
        // Dosya var, aç
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Opening file failed\n");
            return;
        }
        int size = 256;
        char buffer[size];
        int bytesRead;

        while ((bytesRead = read(fd, buffer, size)) > 0) {
            write(STDOUT_FILENO, buffer, bytesRead);
        }
        writeToLog("Done Successfully\n");
        
        


        // Dosyayı kapat
        close(fd);
    } else {
        writeToLog("Error File not be found\n");
        char message[] = "Error: File not be found\n";
        write(STDOUT_FILENO, message, cmdSize(message));
    }
}
// print 5 entries from the file
void listGrades(char *fileName) {
    // Dosyanın var olup olmadığını kontrol et
    if (access(fileName, F_OK) != -1) {
        // Dosya var, aç
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Opening file failed\n");
            return;
        }
        int size = 256;
        char buffer[size];
        int bytesRead;
        int count = 0;

        while ((bytesRead = read(fd, buffer, size)) > 0) {
            char *token = strtok(buffer, "\n");
            while (token != NULL && count < 5) {
                write(STDOUT_FILENO, token, strlen(token));
                write(STDOUT_FILENO, "\n", 1);
                count++;
                token = strtok(NULL, "\n");
            }

            if (count == 5) {
                break;
            }
        }
        writeToLog("Done Successfully\n");
        // Dosyayı kapat
        close(fd);
    } else {
        writeToLog("Error File not be found\n");
        char message[] = "Error: File not be found\n";
        write(STDOUT_FILENO, message, cmdSize(message));
    }
}
//listSome “numofEntries” “pageNumber” “grades.txt” 
//e.g. listSome 5 2 grades.txt command will list entries between 5th and 10th.
void listSome(char *numofEntries, char *pageNumber, char *fileName){
    // Dosyanın var olup olmadığını kontrol et
    if (access(fileName, F_OK) != -1) {
        // Dosya var, aç
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Opening file failed\n");
            return;
        }
        int size = 256;
        char buffer[size];
        int bytesRead;
        int count = 0;
        int start = (atoi(pageNumber) - 1) * atoi(numofEntries);
        
        int end = start + atoi(numofEntries);
        
        int current = 0;
        
        
        bytesRead = read(fd, buffer, size);
        if(bytesRead > 0){
            char *token = strtok(buffer, "\n");
            
            while (token != NULL) {

                if(current >= start && current < end){
                write(STDOUT_FILENO, token, strlen(token));
                write(STDOUT_FILENO, "\n", 1);
                
                }

                current++;
                if(current == end){
                    break;
                }
                token = strtok(NULL, "\n");
            }
    
        }
        writeToLog("Done Successfully\n");
        // Dosyayı kapat
        close(fd);
    } else {
        writeToLog("Error File not be found\n");
        char message[] = "Error: File not be found\n";
        write(STDOUT_FILENO, message, cmdSize(message));
    }

}
void createfile(char *fileName){
    int fd = open(fileName, O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
        pid_t pid=fork();
        if(pid == -1){
            printf("Fork failed\n");
        }
        else if(pid == 0){
            // Example of logging a command execution
            char logMessage[BUFFER_SIZE];
            snprintf(logMessage, BUFFER_SIZE, "File not created: %s\n", fileName);
            writeToLog(logMessage);
            exit(EXIT_SUCCESS);
        }
        else{
            // Ana işlem
            int status;
            wait(&status);
            return;
        }
    }
    pid_t pid=fork();
    if(pid == -1){
        printf("Fork failed\n");
    }
    else if(pid == 0){
        // Example of logging a command execution
        printf("File created: %s\n", fileName);
        char logMessage[BUFFER_SIZE];
        snprintf(logMessage, BUFFER_SIZE, "File created: %s\n", fileName);
        writeToLog(logMessage);
        exit(EXIT_SUCCESS);
    }
    else{
        // Ana işlem
        int status;
        wait(&status);
        close(fd);
    }
    
    
}
int main(int argc, char *argv[]) {
    

    const char * message1 = "Shell (q for exit)>";
	const char * message2 = ": command not found\n";
	char buffer[BUFFER_SIZE];
	int messageFlag = 0;

    while (TRUE) 
	{
        write(STDOUT_FILENO, message1, cmdSize(message1));
        fgets(buffer, BUFFER_SIZE, stdin);
        char *tokens[MAX_TOKENS];
        int tokenCount = 0;
        char *token = buffer;
        while (token != NULL && tokenCount < MAX_TOKENS) {
            if(*token == '\0' || *token == '\n'){
                    
                    break;
            }
            if (*token == '"') {
                char *arr = (char *)malloc(BUFFER_SIZE * sizeof(char));
                if (arr == NULL) {
                    printf("Bellek yetersiz!\n");
                    break;
                }
                int i = 0;
                token++; // İlk çift tırnağı atlıyoruz
                while (*token != '"') {
                    arr[i++] = *token;
                    token++;
                }
                arr[i] = '\0';
            
                tokens[tokenCount++] = arr;
                if(*token == '\0'){
                    break;
                }
                if (*token == '"') // Eğer son tırnak varsa, bir sonraki kelimeye geçmek için bir karakter ileri atlayalım
                    token++;
            }
            else if (*token != ' ') {
                
                char *arr = (char *)malloc(BUFFER_SIZE * sizeof(char));
                if (arr == NULL) {
                    printf("Bellek yetersiz!\n");
                    break;;
                }
                int i = 0;
                while (*token != '\0' && *token != ' ') {
                    arr[i++] = *token;
                    token++;
                }
            
                arr[i] = '\0';
                tokens[tokenCount++] = arr;
            
                if(*token == ' '){
                    token++;
                }
                
            }
            else {
            
                token++;
            }
        }
        

        if(tokenCount>0){
            trim(tokens[0]);   
            if(strcmp(tokens[0],"q") == 0){
                for(int i = 0; i < tokenCount; i++){
                    free(tokens[i]);
                }
                break;
                
            }
            if(strcmp(tokens[0],"gtuStudentGrades") == 0){
                if(tokenCount == 1){
                //print all instructions using write function
                
                char message[] = "gtuStudentGrades without arguments : print all instructions\ngtuStudentGrades <filename> : create a file\naddStudentGrade <Name Surname> <Grade> : Add student grade to file\nsearchStudent <Name Surname> : Search student grade in file\nsortAll <fileName> : Sort all entries in file\nshowAll <fileName> : Show all entries in file\nlistGrades <fileName> : List first 5 entries in file\nlistSome <numofEntries> <pageNumber> <fileName> : List entries between given page number and number of entries\n";
                write(STDOUT_FILENO, message, cmdSize(message));

                pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        // Example of logging a command execution
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command: %s\n", tokens[0]);
                        writeToLog(logMessage);
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                }
                else if(tokenCount == 2){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        pid_t pid=fork();
                        if(pid == -1){
                            printf("Fork failed\n");
                        }
                        else if(pid == 0){
                            // Example of logging a command execution
                            char logMessage[BUFFER_SIZE];
                            snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s", tokens[0],tokens[1]);
                            writeToLog(logMessage);
                            exit(EXIT_SUCCESS);
                        }
                        else{
                            // Ana işlem
                            int status;
                            wait(&status);
                            createfile(tokens[1]);
                            exit(EXIT_SUCCESS);
                        }
                        
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));
                    // Example of logging a command execution with unknown number of tokens
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                        for (int i = 0; i < tokenCount; i++) {
                            snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                        }
                        writeToLog(logMessage);
                        writeToLog("Error: Invalid number of arguments\n");
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                        for(int i = 0; i < tokenCount; i++){
                        free(tokens[i]);
                        }
                    }
                }
               
                
            }
            else if(strcmp(tokens[0],"addStudentGrade") == 0){
                if(tokenCount == 4){
                    pid_t pid=fork();
                    if(pid == -1){
                    printf("Fork failed\n");
                    }
                    else if(pid == 0){ 
                        pid_t pid=fork();
                        if(pid == -1){
                            printf("Fork failed\n");
                        }
                        else if(pid == 0){
                                // Example of logging a command execution
                            char logMessage[BUFFER_SIZE];
                            snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s %s %s", tokens[0],tokens[1],tokens[2],tokens[3]);
                            writeToLog(logMessage);
                            exit(EXIT_SUCCESS);
                        }
                        else{
                            // Ana işlem
                            int status;
                            wait(&status);
                            char *tokensNameSurname[MAX_TOKENS];
                            int tokenNameCount = 0;
                            char *token = strtok(tokens[1], " "); // İlk tokeni al
                            while (token != NULL && tokenNameCount < MAX_TOKENS) {
                                tokensNameSurname[tokenNameCount++] = token; // Tokeni diziye ekle
                                token = strtok(NULL, " "); // Bir sonraki tokeni al
                            }
                            addStudentGrade(tokensNameSurname,tokenNameCount,tokens[2],tokens[3]);
                            exit(EXIT_SUCCESS);
                        }   
                        
                        
                    }
                    else{
                         // Ana işlem
                        int status;
                        wait(&status);
                        
                    }     
                }
                else {
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
                    
            }
            else if(strcmp(tokens[0],"searchStudent") == 0){
                if(tokenCount == 3){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                            char logMessage[BUFFER_SIZE];
                            snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s %s", tokens[0],tokens[1],tokens[2]);
                            writeToLog(logMessage);
                            
                            searchStudent(tokens[1],tokens[2]);
                            exit(EXIT_SUCCESS);
                    }
                    else{
                            // Ana işlem
                            int status;
                            wait(&status);
                        
                    }
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
            }
            else if(strcmp(tokens[0],"sortAll") == 0){
                if(tokenCount == 2){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s", tokens[0],tokens[1]);
                        writeToLog(logMessage);
                        sortAll(tokens[1]);
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
            }
            else if(strcmp(tokens[0],"showAll")==0){
                if(tokenCount == 2){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s", tokens[0],tokens[1]);
                        writeToLog(logMessage);
                        showAll(tokens[1]);
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                    
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
                


            }
            else if(strcmp(tokens[0],"listGrades")==0){
                if(tokenCount == 2){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s", tokens[0],tokens[1]);
                        writeToLog(logMessage);
                        listGrades(tokens[1]);
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                    
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
                
            }
            else if(strcmp(tokens[0],"listSome")==0){
                if(tokenCount == 4){
                    pid_t pid=fork();
                    if(pid == -1){
                        printf("Fork failed\n");
                    }
                    else if(pid == 0){
                        char logMessage[BUFFER_SIZE];
                        snprintf(logMessage, BUFFER_SIZE, "Executed command: %s %s %s %s", tokens[0],tokens[1],tokens[2],tokens[3]);
                        writeToLog(logMessage);
                        listSome(tokens[1],tokens[2],tokens[3]);
                        exit(EXIT_SUCCESS);
                    }
                    else{
                        // Ana işlem
                        int status;
                        wait(&status);
                    }
                }
                else{
                    char message[] = "Error: Invalid number of arguments\n";
                    write(STDOUT_FILENO, message, cmdSize(message));

                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid number of arguments\n");
                }
                
            }
            else {
                write(STDOUT_FILENO, tokens[0], strlen(tokens[0]));
                write(STDOUT_FILENO, message2, cmdSize(message2));
                pid_t pid=fork();
                if(pid == -1){
                    printf("Fork failed\n");
                }
                else if(pid == 0){
                    // Example of logging a command execution with unknown number of tokens
                    char logMessage[BUFFER_SIZE];
                    snprintf(logMessage, BUFFER_SIZE, "Executed command:");
                    for (int i = 0; i < tokenCount; i++) {
                        snprintf(logMessage + strlen(logMessage), BUFFER_SIZE - strlen(logMessage), " %s", tokens[i]);
                    }
                    writeToLog(logMessage);
                    writeToLog("Error: Invalid command entered");
                }
                else{
                    // Ana işlem
                    int status;
                    wait(&status);
                    for(int i = 0; i < tokenCount; i++){
                    free(tokens[i]);
                    }
                }
            }
            
            
        }
        else{
            
            
            write(STDOUT_FILENO, message2, cmdSize(message2));

            pid_t pid=fork();
            if(pid == -1){
                printf("Fork failed\n");
            }
            else if(pid == 0){
                // Example of logging a command execution
                char logMessage[BUFFER_SIZE];
                snprintf(logMessage, BUFFER_SIZE, "Invalid command entered\n");
                writeToLog(logMessage);
                exit(EXIT_SUCCESS);
            }
            else{
                // Ana işlem
                int status;
                wait(&status);
                for(int i = 0; i < tokenCount; i++){
                free(tokens[i]);
                }
            }
        }
          
            
    //clean the buffer
    memset(buffer, 0, BUFFER_SIZE);
    
    }


    return 0;
}