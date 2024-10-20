#include <iostream>

typedef unsigned char byte;

using std::string;
using std::cin;
using std::cout;
using std::endl;

int main (int argc, char *argv[]) {
    byte aux;
    int colu, fila;
    string nombre;
    FILE *f;
    if (argc == 2) {
        nombre = argv[1];
    }
    else {
        cout << "Ingrese el nombre del binario a decompilar (con extension): ";
        getline(cin,nombre);
    }
    f = fopen(nombre.c_str(),"rb+");
    if (f) {
        printf("\n%-8s   %-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c\n\n",
           "OFFSET",'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F');
        for (fila = 0; !feof(f); fila++) {
            printf("%08X   ",0x10 * fila);
            for (colu = 0; colu < 16; colu++) {
                fread(&aux,1,1,f);
                printf("%-04X",aux);
            }
            printf("\n");
        }
        fclose(f);
    }
    else {
        cout << "No se pudo abrir el binario" << endl;
    }
    return 0;
}
