#include <cctype>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#define MAXIDENT 40

using namespace std;

typedef unsigned char byte;

struct Ident {
    string nombre;
    string tipo;
    int valor;

    void mostrar() {
        cout << left <<
        setw(15) << nombre << " " <<
        setw(15) << tipo << " " <<
        setw(15) << valor << endl;
    }
};

struct Sim {
    string valor;
    string tipo;

    bool es (string s) {
        return tipo == s;
    }

    void mostrar() {
        cout << left <<
        setw(70) << valor << " " <<
        setw(12) << tipo << " (s.valor -- s.tipo)" << endl;
    }
};

/**LEXICO*/
Sim escanear (ifstream&);
string pasarAMayus(string);
void esReservada (Sim&);

/**SINTACTICO*/
void programa (ifstream&, Sim&, byte*,int&);
void bloque (ifstream&, Sim&, int, Ident*, byte*, int&, int&);
void proposicion (ifstream&, Sim&, int, Ident*, byte*, int&, int&);
void procesarWrite(ifstream&,Sim&, int, Ident*, byte*, int&, int&);
void condicion (ifstream&, Sim&, int, Ident*, byte*, int&, int&);
void expresion (ifstream&, Sim&, int, Ident*, byte*, int&, int&);
void termino (ifstream&, Sim&, int, Ident*, byte*, int&, int&);
void factor (ifstream&, Sim&, int, Ident*, byte*, int&, int&);

/**SEMANTICO*/
void inicializarTabla(Ident*);
bool verificarID(Ident*, string, int, int);
int buscarID (Ident*, string, int, int);
void mostrarTablaIDs (Ident*);

/**GENERADOR DE CODIGO*/
void iniciarMemoria(byte*,int&);
void cargar32(int,byte*,int&);
void cargar32Fijo(int,byte*,int);
int byte2int(byte*,int,int);
void generarExe(byte*,int,string);

void error (int, string);
void imprimirMemoria(byte*,int);

int main (int argc, char *argv[]) {
    int posVec;
    byte memoria[8192]; //8 KB
    string nombre;
    ifstream arch;
    Sim s;
    system("color 1f");
    if (argc == 2) {
        nombre = argv[1];
    }
    else {
        cout << "Ingrese el nombre del archivo .pl0 a compilar (sin extension): ";
        getline(cin,nombre);
    }
    arch.open(nombre + ".pl0"); // pasar por referencia al archivo
    if (arch.is_open()) {
        cout << "Codigo fuente abierto correctamente." << endl;
        s = escanear(arch);
        programa(arch,s,memoria,posVec);
        generarExe(memoria, posVec, nombre);
        cout << "Compilacion exitosa" << endl;
        arch.close();
        imprimirMemoria(memoria,posVec);
    }
    else {
        cout << "Problemas al abrir el archivo." << endl;
    }
    return 0;
}

void generarExe(byte *memoria, int posVec, string nombre) {
    FILE *f;
    int i;
    nombre = nombre + ".exe";
    f = fopen(nombre.c_str(),"wb+");
    if (f) {
        for (i = 0; i < posVec; i++) {
            fwrite(&(memoria[i]),1,1,f);
        }
    }
    else {
        cout << "Problemas al generar el ejecutable." << endl;
    }
    fclose(f);
}

void imprimirMemoria (byte *memoria, int posVec) {
    int i = 0, c, f;
    printf("\n%-8s   %-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c%-4c\n\n",
           "OFFSET",'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F');
    for (f = 0; i < posVec; f++) {
        printf("%08X   ",0x10 * f);
        for (c = 0; c < 16; c++) {
            printf("%-04X",memoria[i]);
            i++;
        }
        printf("\n");
    }
}

void programa (ifstream& arch, Sim &s, byte *memoria, int &posVec) {
    int contVar = 0, dist, dist2, i, origen,
        baseOfCode, imageBase, baseOfData, sizeOfImage,
        sizeOfCodeSection, sizeOfRawData, sectionAlignment;
    Ident tablaID[MAXIDENT - 1];
    inicializarTabla(tablaID);
    iniciarMemoria(memoria, posVec);
    memoria[posVec++] = 0xBF; // MOV EDI, ...
    origen = posVec; // 1793
    cargar32(0,memoria,posVec); // "debera reservarse el lugar grabando BF 00 00 00 00"
    bloque(arch,s,0,tablaID, memoria, posVec, contVar);
    if (s.es("_PTO")) {
        s = escanear(arch);
    }
    else {
        error(0, s.valor); // ERROR DE PUNTO
    }
    if (!s.es("EOF")) {
        error(10, s.valor); // ERROR DE EOF
    }
    dist = 1416 - posVec - 5; // llamado a rutina final
    memoria[posVec++] = 0xE9; // JMP dir
    cargar32(dist,memoria,posVec);
    baseOfCode = byte2int(memoria,204,207);
    imageBase = byte2int(memoria,212,215);
    dist2 = posVec + baseOfCode + imageBase - 0x200;
    cargar32Fijo(dist2,memoria,origen); // cargo el desplazamiento
    /**INICIO DE LA SECCION DE VARIABLES */
    for (i = 0; i < contVar; i++) {
        cargar32(0,memoria,posVec);
    }
    cargar32Fijo(posVec-0x200,memoria,416);
    /** Ajuste de File Alignment*/
    while ((posVec % 512) != 0) {
        memoria[posVec++] = 0;
    }
    /** Ajuste de Size of Code Section*/
    sizeOfCodeSection = posVec - 0x200;
    cargar32Fijo(sizeOfCodeSection,memoria,188);
    /** Ajuste de Size of Raw Data*/
    sizeOfRawData = posVec - 0x200;
    cargar32Fijo(posVec-0x200,memoria,424);
    /** Ajuste de Size of Image y Size of Raw Data */
    sectionAlignment = byte2int(memoria,216,219);
    sizeOfImage = (int) (2 + sizeOfCodeSection/sectionAlignment) * sectionAlignment;
    baseOfData = (int) (2 + sizeOfRawData/sectionAlignment) * sectionAlignment;
    cargar32Fijo(sizeOfImage,memoria,240);
    cargar32Fijo(baseOfData,memoria,208);
    mostrarTablaIDs(tablaID);
    //cout << "Cantidad de variables: " << contVar << endl;
}

void bloque (ifstream &arch, Sim &s, int base, Ident *tablaID, byte *memoria, int &posVec, int &contVar) {
    int despl = 0, origen, signo;
    Ident i;
    memoria[posVec++] = 0xE9; // JMP dir
    cargar32(0,memoria,posVec);
    origen = posVec;
    if (s.es("CONST")) {
        do {
            signo = 1;
            s = escanear(arch);
            if (s.es("_IDENT")) {
                i.nombre = s.valor;
                s = escanear(arch);
            }
            else {
                error(1, s.valor); // ERROR DE IDENT
            }
            if (s.es("_IGUAL")) {
                s = escanear(arch);
            }
            else {
                error(3, s.valor); // ERROR DE IGUAL
            }
            if (s.es("_MENOS")) { // agregado por FOR TO (2do rec 2019)
                signo = -1;
                s = escanear(arch);
            }
            if (s.es("_NUM")) {
                i.valor = signo * atoi(s.valor.c_str());
                s = escanear (arch);
            }
            else {
                error(4, s.valor); // ERROR DE NUMERO
            }
            if (verificarID(tablaID, i.nombre, base, base + despl - 1)) {
                i.tipo = "CONST";
                tablaID[base+despl] = i;
                despl++;
            }
            else {
                error(17, s.valor); // identificador repetido en scope
            }
        } while (s.es("_COMA"));
        if (s.es("_PTOYCOMA")) {
            s = escanear(arch);
        }
        else {
            error(9, s.valor); // ERROR DE PTO Y COMA o COMA
        }
    }
    if (s.es("VAR")) {
        do {
            i.tipo = "VAR";
            s = escanear(arch);
            if (s.es("_IDENT")) {
                i.nombre = s.valor;
                s = escanear(arch);
            }
            else {
                error(1, s.valor); // ERROR DE IDENT
            }

            if (verificarID(tablaID, i.nombre, base, base + despl - 1)) {
                i.valor = contVar * 4;
                tablaID[base+despl] = i;
                contVar++;
                despl++;
            }
            else {
                error(17, s.valor); // identificador repetido en scope
            }
        } while (s.es("_COMA"));
        if (s.es("_PTOYCOMA")) {
            s = escanear(arch);
        }
        else {
            error(9, s.valor); // ERROR DE PTO Y COMA o COMA
        }
    }
    while (s.es("PROCEDURE")) {
        s = escanear(arch);
        if (s.es("_IDENT")) {
            i.nombre = s.valor;
            i.tipo = "PROCEDURE";
            i.valor = posVec;
            s = escanear(arch);
        }
        else {
            error(1, s.valor); // ERROR DE IDENT
        }
        if (s.es("_PTOYCOMA")) {
            s = escanear(arch);
        }
        else {
            error(2, s.valor); // ERROR DE PTO Y COMA
        }
        if (verificarID(tablaID, i.nombre, base, base + despl - 1)) {
            tablaID[base+despl] = i;
        }
        else {
            error(17, s.valor); // identificador repetido en scope
        }
        despl++;
        bloque(arch,s,base+despl,tablaID,memoria,posVec, contVar);
        memoria[posVec++] = 0xC3; // RET
        if (s.es("_PTOYCOMA")) {
            s = escanear(arch);
        }
        else {
            error(2, s.valor); // ERROR DE PTO Y COMA
        }
    }
    int dist = posVec - origen;
    cargar32Fijo(dist,memoria,origen-4);
    proposicion(arch,s,base + despl, tablaID, memoria, posVec, contVar);
}

void proposicion (ifstream &arch, Sim &s, int techo, Ident *tablaID, byte *memoria, int &posVec, int &contVar) {
    int i;
    if (s.es("_IDENT")) {
        i = buscarID(tablaID,s.valor,0,techo - 1);
        if (i == -1) {
            error(19,s.valor);
        }
        if (tablaID[i].tipo != "VAR") {
            error(21,tablaID[i].tipo);
        }
        s = escanear(arch);
        if (s.es("_DEFIN")) {
            s = escanear(arch);
        }
        else {
            error(5, s.valor); // ERROR DE ASIGNACION
        }
        expresion(arch,s, techo,tablaID,memoria,posVec, contVar);
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0x89; // MOV [EDI + ... ], EAX (2 BYTES)
        memoria[posVec++] = 0x87;
        cargar32(tablaID[i].valor,memoria, posVec);
        return;
    }
    else if (s.es("CALL")) {
        s = escanear(arch);
        if (s.es("_IDENT")) {
            i = buscarID(tablaID,s.valor,0,techo - 1);
            if (i == -1) {
                error(19,s.valor);
            }
            if (tablaID[i].tipo != "PROCEDURE") {
                error(20,tablaID[i].tipo); // ERROR DE PROCEDURE
            }
            int dist = tablaID[i].valor - posVec - 5;
            memoria[posVec++] = 0xE8; // CALL DIR
            cargar32(dist,memoria,posVec);
            s = escanear(arch);
            return;
        }
        else {
            error(1, s.valor); // ERROR DE IDENT
        }

    }
    else if (s.es("BEGIN")) {
        do {
            s = escanear(arch);
            proposicion(arch,s,techo,tablaID,memoria,posVec, contVar);
        } while (s.es("_PTOYCOMA"));
        if (s.es("END")) {
            s = escanear(arch);
            return;
        }
        else {
            error(11, s.valor); // ERROR DE END o PUNTO Y COMA
        }
    }
    else if (s.es("IF")) {
        int origen, dist;
        s = escanear(arch);
        condicion(arch,s,techo,tablaID,memoria,posVec, contVar);
        origen = posVec;
        if (s.es("THEN")) {
            s = escanear(arch);
        }
        else {
            error(12, s.valor); // ERROR DE THEN
        }
        proposicion(arch,s,techo,tablaID,memoria,posVec, contVar);
        dist = posVec - origen;
        cargar32Fijo(dist,memoria,origen - 4); // E9 00 00 00 00
        return;
    }
    else if (s.es("WHILE")) {
        int dist, preCon, posCon;
        s = escanear(arch);
        preCon = posVec;
        condicion(arch,s,techo,tablaID,memoria,posVec, contVar);
        posCon = posVec;
        if (s.es("DO")) {
            s = escanear(arch);
        }
        else {
            error(13, s.valor); // ERROR DE DO
        }
        proposicion(arch,s,techo,tablaID,memoria,posVec, contVar);
        dist = preCon - posVec - 5; // punto ANTES de CONDICION
        memoria[posVec++] = 0xE9; // JMP dir
        cargar32(dist,memoria,posVec);
        dist = posVec - posCon;
        cargar32Fijo(dist,memoria,posCon - 4); // E9 00 00 00 00
        return;
    }
    else if (s.es("READLN")) {
        int dist;
        s = escanear(arch);
        if (!s.es("_PARAPE")) {
            error(14, s.valor); // ERROR DE PARAPE
        }
        do {
            s = escanear(arch);
            if (s.es("_IDENT")) {
                i = buscarID(tablaID,s.valor,0,techo-1);
                if (i == -1) {
                    error(19,s.valor);
                }
                if (tablaID[i].tipo == "VAR") {
                    dist = 1424 - posVec - 5;
                    memoria[posVec++] = 0xE8; // CALL dir
                    cargar32(dist,memoria,posVec);
                    memoria[posVec++] = 0x89; // MOV [EDI + ...], EAX (2 BYTES)
                    memoria[posVec++] = 0x87;
                    cargar32(tablaID[i].valor,memoria,posVec);
                }
                else {
                    error(21,tablaID[i].tipo);
                }
                s = escanear(arch);
            }
            else {
                error(1, s.valor); // ERROR DE IDENT
            }
        } while (s.es("_COMA"));
        if (s.es("_PARCIE")) {
            s = escanear(arch);
            return;
        }
        else {
            error(6, s.valor); // ERROR DE PARCIE
        }
    }
    else if (s.es("WRITELN")) {
        int dist, posComp;
        s = escanear(arch);
        if (s.es("_PARAPE")) {
            procesarWrite(arch,s,techo, tablaID, memoria,posVec, contVar);
            dist = 1040 - posVec - 5; // llamado a salto de linea
            memoria[posVec++] = 0xE8; // CALL dir
            cargar32(dist,memoria,posVec);
            return;
        }
        else {
            dist = 1040 - posVec - 5; // llamado a salto de linea
            memoria[posVec++] = 0xE8; // CALL dir
            cargar32(dist,memoria,posVec);
            return;
        }
    }
    else if (s.es("WRITE")) {
        s = escanear(arch);
        if (s.es("_PARAPE")) {
            procesarWrite(arch,s,techo,tablaID, memoria, posVec, contVar);
            return;
        }
        else {
            error(6, s.valor); // ERROR DE PARCIE
        }
    }
    else if (s.es("FOR")) { // agregado 2do Rec 2019
        int posComp, posSalto, valor;
        s = escanear(arch);
        if (s.es("_IDENT")) {
            i = buscarID(tablaID,s.valor,0,techo-1);
        }
        else {
            error(1,s.tipo); // error de ident
        }
        if (tablaID[i].tipo == "VAR") {
            s = escanear(arch);
        }
        else {
            error(21,tablaID[i].tipo); // error de ident var
        }
        if (s.es("_DEFIN")) {
            s = escanear(arch);
        }
        else {
            error(5, s.valor); // ERROR DE ASIGNACION
        }
        expresion(arch,s,techo,tablaID,memoria,posVec,contVar);
        memoria[posVec++] = 0x58; // POP EAX para obtener el 1er numero del FOR desde expresion                
        memoria[posVec++] = 0x89; // MOV [EDI + ... ], EAX (2 BYTES)
        memoria[posVec++] = 0x87;
        cargar32(tablaID[i].valor,memoria, posVec);
        if (s.es("TO")) {
            s = escanear(arch);
        }
        else {
            error(22,s.valor); // error de TO
        }
        expresion(arch,s,techo,tablaID,memoria,posVec,contVar);
        memoria[posVec++] = 0x5B; // POP EBX para obtener el 2do numero del FOR desde expresion
        if (s.es("DO")) {
            s = escanear(arch);
        }
        else {
            error(13,s.valor); // error de DO
        }
        memoria[posVec++] = 0x8B; // MOV EAX, [EDI + ...] para mover a EAX el 1er num del FOR
        memoria[posVec++] = 0x87; 
        cargar32(tablaID[i].valor,memoria, posVec);
        posComp = posVec; // es a la posicion en memoria a la que voy a volver despues de llamar a proposicion
        /**EMPIEZO LA COMPARACION PARA EL CICLO FOR*/
        memoria[posVec++] = 0x39; // CMP EBX, EAX (comparacion)
        memoria[posVec++] = 0xC3;
        memoria[posVec++] = 0x7D; // JGE (JUMP IF EBX >= EAX)
        memoria[posVec++] = 0x05; // Si EBX es mas grande, salta 5 bytes para incrementar
        memoria[posVec++] = 0xE9; // JMP dir, salto incondicional de 4 bytes
        cargar32(0,memoria,posVec); // distancia a saltar (reservo lugar, fixear despues)
        posSalto = posVec;
        /**INCREMENTAR EAX*/
        memoria[posVec++] = 0x93; // XCHG EAX, EBX para que en EAX quede el 2do num del FOR
        memoria[posVec++] = 0x50; // "PUSH EBX", PUSH EAX para guardar el 2do num antes de entrar a proposicion
        memoria[posVec++] = 0x93; // XCHG EAX, EBX para que en EAX quede el 1er num del FOR
        memoria[posVec++] = 0x50; // PUSH EAX para guardar el 1er num antes de entrar a proposicion
        proposicion(arch,s,techo,tablaID,memoria,posVec,contVar);
        memoria[posVec++] = 0x5B; // POP EBX para recuperar el 1er num del FOR
        memoria[posVec++] = 0xB8; // MOV EAX, 1 (para incrementar);
        cargar32(1,memoria,posVec);
        memoria[posVec++] = 0x01; // ADD EAX, EBX
        memoria[posVec++] = 0xD8; // (eax = eax + 1)
        memoria[posVec++] = 0x5B; // POP EBX para recuperar el 2do num del FOR
        memoria[posVec++] = 0xE9; // JMP dir (salto incondicional para volver a comparar)
        cargar32(posComp-posVec-5,memoria,posVec);
        cargar32Fijo(posVec - posSalto,memoria,posSalto - 4); // fix up del salto
        return;
    }
    else if (s.es("HALT")) {
        /** llamado a rutina final*/
        int dist = 1416 - posVec - 5; // 1416: finaliza el programa
        memoria[posVec++] = 0xE8;
        cargar32(dist,memoria,posVec);
        s = escanear(arch);
        return;
    }
    else if (s.es("SUCC")) {
        s = escanear(arch);
        if (s.es("_PARAPE")) {
            s = escanear(arch);
        }
        else {
            error(14, s.valor); // ERROR DE PARAPE
        }
        if (s.es("_IDENT")) {
            i = buscarID(tablaID,s.valor,0,techo-1);
            if (i == -1) {
                error(19,s.valor); // ERROR DE IDENT NO DECLARADO
            }
            if (tablaID[i].tipo != "VAR") {
                error(21,s.tipo); // ERROR DE IDENT DE VARIABLE
            }
        }
        else if (s.es("_NUM")) {
            //a completar
        }
        else {
            error(1,s.tipo); // ERROR DE NO IDENT
        }
        /** busco el valor en memoria y lo cargo en EAX*/
        memoria[posVec++] = 0x8B; // MOV EAX, [EDI + ...] para traer a EAX el valor segun su direccion
        memoria[posVec++] = 0x87;
        cargar32(tablaID[i].valor,memoria,posVec);
        memoria[posVec++] = 0x93; // XCHG EAX, EBX, muevo el valor a EBX para poder preparar la suma
        memoria[posVec++] = 0xB8; // MOV EAX, 01
        cargar32(1,memoria,posVec); // muevo un 1 a EAX para poder incrementar
        memoria[posVec++] = 0x01; // ADD EAX, EBX
        memoria[posVec++] = 0xD8; // con EAX = 1 y EBX = valor
        memoria[posVec++] = 0x89; // MOV [EDI + ...], EAX
        memoria[posVec++] = 0x87; // para mover el contenido de EAX a la direccion EDI + ...
        cargar32(tablaID[i].valor,memoria,posVec);
        s = escanear(arch);
        if (s.es("_PARCIE")) {
            s = escanear(arch);
        }
        else {
            error(6,s.valor);
        }
        return;
    }
    else {
        return;
    }
}

void procesarWrite (ifstream &arch, Sim &s, int techo, Ident *tablaID, byte *memoria, int &posVec, int &contVar) {
    do {
        s = escanear(arch);
        if (s.es("_CADENA")) {
            int i;
            int baseOfCode = byte2int(memoria, 204, 207);
            int imageBase = byte2int(memoria, 212, 215);
            int pos = posVec + baseOfCode + imageBase + 15 - 0x200; // duda
            int largoCad = s.valor.length()-1; // saco comillas
            memoria[posVec++] = 0xB8; // MOV EAX
            cargar32(pos,memoria,posVec); // cargo posicion en EAX
            int dist = 992 - posVec - 5;
            memoria[posVec++] = 0xE8; // CALL dir
            cargar32(dist,memoria,posVec);
            memoria[posVec++] = 0xE9; // JMP dir
            cargar32(largoCad,memoria,posVec); //
            for (i = 1; i < largoCad; i++) { // arranca en 1 por la comilla de apertura
                memoria[posVec++] = s.valor[i];
            }
            memoria[posVec++] = 0x00; // '\0'
            s = escanear(arch);
        }
        else {
            expresion(arch,s,techo,tablaID,memoria, posVec, contVar);
            memoria[posVec++] = 0x58; // POP EAX;
            int dist = 1056 - posVec - 5;
            memoria[posVec++] = 0xE8; // CALL DIR
            cargar32(dist,memoria,posVec);
        }
    } while (s.es("_COMA"));
    if (s.es("_PARCIE")) {
        s = escanear(arch);
        return;
    }
    else {
        error(6, s.valor); // ERROR DE PARCIE
    }
}

void condicion (ifstream &arch, Sim &s, int techo, Ident* tablaID, byte *memoria, int &posVec, int &contVar) {
    string comp;
    if (s.es("ODD")) {
        s = escanear(arch);
        expresion(arch,s,techo, tablaID, memoria, posVec, contVar);
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0xA8; // TEST AL, ab (DATA, 2 BYTES)
        memoria[posVec++] = 0x01; // 01 hardcodeado, distancia del salto
        memoria[posVec++] = 0x7B; // JPO dir (DATA, 2 BYTES, SALTO IF ODD)
        memoria[posVec++] = 0x05; // 05 hardcodeado, distancia del salto
        memoria[posVec++] = 0xE9; // JMP dir (DATA, 2 BYTES, SALTO INCONDICIONAL)
        cargar32(0,memoria,posVec);
        return;
    }
    else {
        expresion(arch,s,techo, tablaID, memoria, posVec, contVar);
        if (s.es("_IGUAL") || s.es("_DISTINT") ||
            s.es("_MENOR") || s.es("_MENORIG") ||
            s.es("_MAYOR") || s.es("_MAYORIG")) {
            comp = s.tipo; // para no perderlo despues de llamar a expresion
            s = escanear(arch);
        }
        else {
            error(15, s.valor); // ERROR DE SimS DE COMPARACION
        }
        expresion(arch,s,techo, tablaID, memoria, posVec, contVar);
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0x5B; // POP EBX
        memoria[posVec++] = 0x39; // CMP EBX, EAX (2 BYTES)
        memoria[posVec++] = 0xC3;
        if (comp == "_IGUAL") {
            memoria[posVec++] = 0x74; // JE, JZ
        }
        else if (comp == "_DISTINT") {
            memoria[posVec++] = 0x75; // JNE, JNZ
        }
        else if (comp == "_MENOR") {
            memoria[posVec++] = 0x7C; // JL
        }
        else if (comp == "_MENORIG") {
            memoria[posVec++] = 0x7E; // JLE
        }
        else if (comp == "_MAYOR") {
            memoria[posVec++] = 0x7F; // JG
        }
        else if (comp == "_MAYORIG") {
            memoria[posVec++] = 0x7D; // JGE
        }
        memoria[posVec++] = 0x05; // 05 hardcodeado, distancia del salto
        memoria[posVec++] = 0xE9; // JMP dir (DATA, 2 BYTES)
        cargar32(0,memoria,posVec); // 0x00 * 4
        return;
    }
}

void expresion (ifstream &arch, Sim &s, int techo, Ident* tablaID, byte *memoria, int &posVec, int &contVar) {
    string op;
    if (s.es("_MAS")) {
        s = escanear(arch);
    }
    else if (s.es("_MENOS")) {
        op = "-";
        s = escanear(arch);
    }
    termino(arch,s,techo,tablaID, memoria, posVec, contVar);
    if (op == "-") {
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0xF7; // NEG EAX (2 BYTES)
        memoria[posVec++] = 0xD8;
        memoria[posVec++] = 0x50; // PUSH EAX
    }
    while(s.es("_MAS") || s.es("_MENOS")) {
        op = s.valor;
        s = escanear(arch);
        termino(arch,s,techo,tablaID, memoria, posVec, contVar);
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0x5B; // POP EBX
        if (op == "+") {
            memoria[posVec++] = 0x01; // ADD EAX, EBX (2 BYTES)
            memoria[posVec++] = 0xD8;
        }
        else if (op == "-") {
            memoria[posVec++] = 0x93; // XCHG EAX, EBX
            memoria[posVec++] = 0x29; // SUB EAX, EBX (2 BYTES)
            memoria[posVec++] = 0xD8;
        }
        memoria[posVec++] = 0x50; // PUSH EAX
    }
}

void termino (ifstream &arch, Sim &s, int techo, Ident *tablaID, byte *memoria, int &posVec, int &contVar) {
    string op;
    factor(arch,s,techo,tablaID, memoria, posVec, contVar);
    while (s.es("_ASTER") || s.es("_BARRA")) {
        op = s.tipo;
        s = escanear(arch);
        factor(arch,s,techo,tablaID, memoria, posVec, contVar);
        memoria[posVec++] = 0x58; // POP EAX
        memoria[posVec++] = 0x5B; // POP EBX
        if (op == "_ASTER") {
            memoria[posVec++] = 0xF7; // IMUL EBX (2 BYTES)
            memoria[posVec++] = 0xEB;
        }
        else {
            memoria[posVec++] = 0x93; // XCHG EAX, EBX
            memoria[posVec++] = 0x99; // CDQ (CONVERT DOUBLE TO QUADRUPLE)
            memoria[posVec++] = 0xF7; // IDIV EBX (2 BYTES)
            memoria[posVec++] = 0xFB;
        }
        memoria[posVec++] = 0x50; // PUSH EAX;
    }
}

void factor (ifstream &arch, Sim &s, int techo, Ident *tablaID, byte *memoria, int &posVec, int &contVar) {
    int i;
    if (s.es("_IDENT")) {
        i = buscarID(tablaID,s.valor,0,techo-1);
        if (tablaID[i].tipo == "PROCEDURE") {
            error(18, s.valor); // se esperaba identificador que no sea procedure
        }
        if (tablaID[i].tipo == "CONST") {
            memoria[posVec++] = 0xB8; // MOV EAX, BYTES
        }
        else { // "VAR"
            memoria[posVec++] = 0x8B; // MOV EAX, [EDI + BYTES]
            memoria[posVec++] = 0x87;
        }
        cargar32(tablaID[i].valor,memoria,posVec);
        memoria[posVec++] = 0x50; // PUSH EAX
        s = escanear(arch);
    }
    else if (s.es("_NUM")) {
        memoria[posVec++] = 0xB8; // MOV EAX, BYTES
        cargar32(atoi(s.valor.c_str()),memoria,posVec);
        memoria[posVec++] = 0x50; // PUSH EAX
        s = escanear(arch);
    }
    else if (s.es("_PARAPE")) {
        s = escanear(arch);
        expresion(arch,s,techo,tablaID, memoria, posVec, contVar);
        if (s.es("_PARCIE")) {
            s = escanear(arch);
            return;
        }
        else {
            error(6, s.valor); // ERROR DE PARCIE
        }
    }
    else {
        error(8, s.valor); // ERROR DE IDENT, NUM o PARAPE
    }
}

bool verificarID (Ident *tabla, string nombre, int base, int techo) {
    bool flag = true;
    while (base <= techo) {
        if (strcmpi(tabla[techo].nombre.c_str(), nombre.c_str()) == 0) {
        //if (tabla[techo].nombre == nombre) {
            flag = false;
            break;
        }
        techo--;
    }
    return flag;
}

int buscarID (Ident *tabla, string ident, int base, int techo) {
    while (base <= techo) { // if (strcmpi(tabla[techo].nombre.c_str(), ident.c_str() == 0) {
        if (strcmpi(tabla[techo].nombre.c_str(), ident.c_str()) == 0) {
        //if (tabla[techo].nombre == ident) {
            return techo;
        }
        techo--;
    }
    return -1;
}

void esReservada (Sim &s) { // agregado FOR y T0 (2do rec 2019), HALT (2do rec 2022)
    bool flag = false;
    int i;
    string r[] = {"CONST", "VAR", "PROCEDURE", "CALL", "BEGIN",
                  "END", "IF", "THEN", "DO", "WHILE",
                  "ODD", "WRITE", "WRITELN", "READLN", "FOR",
                  "TO", "HALT","SUCC"},
           t = pasarAMayus(s.valor);
    for (i = 0; i < 18; i++) {
        if (t == r[i]) {
            flag = true;
            break;
        }
    }
    flag? s.tipo = r[i] : s.tipo = "_IDENT";
    return;
}

string pasarAMayus (string s) {
    int i;
    string t = "";
    for (i = 0; i < s.length(); i++) {
        t.push_back(toupper(s[i]));
    }
    return t;
}

void mostrarTablaIDs (Ident *tabla) {
    int i;
    cout << left << setw(15) << "nombre" << " " << setw(15) << "tipo" << " " << setw(15) << "valor" << endl;
    for (i = 0; (i < MAXIDENT - 1); i++) {
        if (tabla[i].nombre != "")
            tabla[i].mostrar();
    }
}

void cargar32 (int valor, byte *memoria, int &posVec) {
    memoria[posVec++] = (byte) (valor >> 0 ) & 0xFF;
    memoria[posVec++] = (byte) (valor >> 8 ) & 0xFF;
    memoria[posVec++] = (byte) (valor >> 16) & 0xFF;
    memoria[posVec++] = (byte) (valor >> 24) & 0xFF;
}

void cargar32Fijo (int valor, byte *memoria, int pos) {
    memoria[pos++] = (byte) (valor >> 0 ) & 0xFF;
    memoria[pos++] = (byte) (valor >> 8 ) & 0xFF;
    memoria[pos++] = (byte) (valor >> 16) & 0xFF;
    memoria[pos++] = (byte) (valor >> 24) & 0xFF;
}

int byte2int (byte *memoria, int base, int techo) {
    int f = 0, acum = 0;
    while (base <= techo) {
        acum = acum + (memoria[base++] << (8 * f++)) ;
    }
    return acum;
}

void error (int cod, string Sim)  {
    char listaError[23][60] = {
        "punto", // 0
        "identificador",// 1
        "punto y coma", // 2
        "signo igual", // 3
        "numero", // 4
        "asignacion", // 5
        "parentesis de cierre", // 6
        "END", // 7
        "identificador, numero o parentesis de apertura", // 8
        "coma o punto y coma", // 9
        "EOF", // 10
        "END o punto y coma", // 11
        "THEN", // 12
        "DO", // 13
        "parentesis de apertura", // 14
        "Sim de comparacion", // 15
        "cadena", // 16
        "identificador no repetido (error de scope)", // 17
        "identificador de constante o variable", // 18
        "identificador declarado", // 19
        "identificador de procedimiento", // 20
        "identificador de variable" // 21
        };
    cout << "Error: se esperaba " << listaError[cod] << ", se obtuvo: " << Sim << endl;
    exit(cod);
}

Sim escanear(ifstream &arch) {
    char c;
    Sim s;
    if (arch.eof()) {
        s.tipo = "EOF";
        s.valor = "EOF";
    }
    else {
        do {
            arch.get(c);
            if(arch.eof()) {
                s.tipo = "EOF";
                s.valor = "EOF";
                break;
            }
        } while (isspace(c));
        if (isalpha(c)) {
            do {
                s.valor.push_back(c);
                arch.get(c);
            } while (isalnum(c));
            arch.unget();
            esReservada(s);
        }
        else if (isdigit(c)) {
            do {
                s.valor.push_back(c);
                arch.get(c);
            } while (isdigit(c));
            s.tipo = "_NUM";
            arch.unget();
        }
        else {
            switch (c) {
                case ':':
                            s.valor.push_back(c);
                            arch.get(c);
                            if (c == '=') {
                                s.tipo = "_DEFIN";
                                s.valor.push_back(c);
                            }
                            else {
                                s.tipo = "_NUL";
                                arch.unget();
                            }
                            break;
                case '<':
                            s.valor.push_back(c);
                            arch.get(c);
                            if (c == '=') { // es menor o igual
                                s.tipo = "_MENORIG";
                                s.valor.push_back(c);
                            }
                            else if (c == '>') { // es distinto
                                s.tipo = "_DISTINT";
                                s.valor.push_back(c);
                            }
                            else {
                                s.tipo = "_MENOR";
                                arch.unget();
                            }
                            break;
                case '>':
                            s.valor.push_back(c);
                            arch.get(c);
                            if (c == '=') { // MAYOR O IGUAL
                                s.tipo = "_MAYORIG";
                                s.valor.push_back(c);
                            }
                            else {
                                s.tipo = "_MAYOR";
                                arch.unget();
                            }
                            break;
                case '.':   s.tipo = "_PTO";
                            s.valor.push_back(c);
                            break;
                case ';':
                            s.tipo = "_PTOYCOMA";
                            s.valor.push_back(c);
                            break;
                case '=':
                            s.tipo = "_IGUAL";
                            s.valor.push_back(c);
                            break;
                case '(':
                            s.tipo = "_PARAPE";
                            s.valor.push_back(c);
                            break;
                case ')':
                            s.tipo = "_PARCIE";
                            s.valor.push_back(c);
                            break;
                case ',':
                            s.tipo = "_COMA";
                            s.valor.push_back(c);
                            break;
                case '/':
                            s.tipo = "_BARRA";
                            s.valor.push_back(c);
                            break;
                case '*':
                            s.tipo = "_ASTER";
                            s.valor.push_back(c);
                            break;
                case '+':
                            s.tipo = "_MAS";
                            s.valor.push_back(c);
                            break;
                case '-':   // REEMPLAZADO POR LA MODIFICACION EN BLOQUE, SUBGRAFO CONST
                            s.tipo = "_MENOS";
                            s.valor.push_back(c);
                            /**
                            s.valor.push_back(c);
                            arch.get(c);
                            if (isdigit(c)) {
                                do {
                                    s.valor.push_back(c);
                                    arch.get(c);
                                } while (isdigit(c));
                                s.tipo = "_NUM";
                            }
                            else {
                                s.tipo = "_MENOS";
                            }
                            arch.unget();
                            */
                            break;
                case '\n':
                            //s.valor.push_back(c);
                            break;
                case '\'': // para caracteres especiales usar el backslash
                            s.valor.push_back(c);
                            arch.get(c);
                            while (c != '\'') {
                                if (c == '\n' || arch.eof()) {
                                    s.tipo = ("NUL");
                                    s.valor.push_back(c);
                                    error(16, s.valor); // SE ESPERABA CADENA
                                    break; // aca deberia romper y no devolver CADENA
                                }
                                else {
                                    s.valor.push_back(c);
                                    arch.get(c);
                                }
                            }
                            if (c == '\'') {
                                s.valor.push_back(c);
                                s.tipo = "_CADENA";
                            }
                            break;
            }
        }
    }
    return s;
}

void inicializarTabla (Ident *tabla) {
    int i;
    for (i = 0; i < MAXIDENT - 1; i++) {
        tabla[i].nombre = "";
        tabla[i].tipo = "";
        tabla[i].valor = -1;
    }
}

void iniciarMemoria (byte *memoria, int &posVec) {
    int i;
    /* MS-DOS COMPATIBLE HEADER */
    memoria[0] = 0x4D; // 'M' (Magic number)
    memoria[1] = 0x5A; // 'Z'
    memoria[2] = 0x60; // Bytes on last block
    memoria[3] = 0x01; // (1 bl. = 512 bytes)
    memoria[4] = 0x01; // Number of blocks
    memoria[5] = 0x00; // in the EXE file
    memoria[6] = 0x00; // Number of relocation entries
    memoria[7] = 0x00; //
    memoria[8] = 0x04; // Size of header
    memoria[9] = 0x00; // (x 16 bytes)
    memoria[10] = 0x00; // Minimum extra
    memoria[11] = 0x00; // paragraphs needed
    memoria[12] = 0xFF; // Maximum extra
    memoria[13] = 0xFF; // paragraphs needed
    memoria[14] = 0x00; // Initial (relative)
    memoria[15] = 0x00; // SS value
    memoria[16] = 0x60; // Initial SP value
    memoria[17] = 0x01;
    memoria[18] = 0x00; // Checksum
    memoria[19] = 0x00;
    memoria[20] = 0x00; // Initial IP value
    memoria[21] = 0x00;
    memoria[22] = 0x00; // Initial (relative)
    memoria[23] = 0x00; // CS value
    memoria[24] = 0x40; // Offset of the 1st
    memoria[25] = 0x00; // relocation item
    memoria[26] = 0x00; // Overlay number.
    memoria[27] = 0x00; // (0 = main program)
    memoria[28] = 0x00; // Reserved word
    memoria[29] = 0x00;
    memoria[30] = 0x00; // Reserved word
    memoria[31] = 0x00;
    memoria[32] = 0x00; // Reserved word
    memoria[33] = 0x00;
    memoria[34] = 0x00; // Reserved word
    memoria[35] = 0x00;
    memoria[36] = 0x00; // OEM identifier
    memoria[37] = 0x00;
    memoria[38] = 0x00; // OEM information
    memoria[39] = 0x00;
    memoria[40] = 0x00; // Reserved word
    memoria[41] = 0x00;
    memoria[42] = 0x00; // Reserved word
    memoria[43] = 0x00;
    memoria[44] = 0x00; // Reserved word
    memoria[45] = 0x00;
    memoria[46] = 0x00; // Reserved word
    memoria[47] = 0x00;
    memoria[48] = 0x00; // Reserved word
    memoria[49] = 0x00;
    memoria[50] = 0x00; // Reserved word
    memoria[51] = 0x00;
    memoria[52] = 0x00; // Reserved word
    memoria[53] = 0x00;
    memoria[54] = 0x00; // Reserved word
    memoria[55] = 0x00;
    memoria[56] = 0x00; // Reserved word
    memoria[57] = 0x00;
    memoria[58] = 0x00; // Reserved word
    memoria[59] = 0x00;
    memoria[60] = 0xA0; // Start of the COFF
    memoria[61] = 0x00; // header
    memoria[62] = 0x00;
    memoria[63] = 0x00;
    memoria[64] = 0x0E; // PUSH CS
    memoria[65] = 0x1F; // POP DS
    memoria[66] = 0xBA; // MOV DX,000E
    memoria[67] = 0x0E;
    memoria[68] = 0x00;
    memoria[69] = 0xB4; // MOV AH,09
    memoria[70] = 0x09;
    memoria[71] = 0xCD; // INT 21
    memoria[72] = 0x21;
    memoria[73] = 0xB8; // MOV AX,4C01
    memoria[74] = 0x01;
    memoria[75] = 0x4C;
    memoria[76] = 0xCD; // INT 21
    memoria[77] = 0x21;
    memoria[78] = 0x54; // 'T'
    memoria[79] = 0x68; // 'h'
    memoria[80] = 0x69; // 'i'
    memoria[81] = 0x73; // 's'
    memoria[82] = 0x20; // ' '
    memoria[83] = 0x70; // 'p'
    memoria[84] = 0x72; // 'r'
    memoria[85] = 0x6F; // 'o'
    memoria[86] = 0x67; // 'g'
    memoria[87] = 0x72; // 'r'
    memoria[88] = 0x61; // 'a'
    memoria[89] = 0x6D; // 'm'
    memoria[90] = 0x20; // ' '
    memoria[91] = 0x69; // 'i'
    memoria[92] = 0x73; // 's'
    memoria[93] = 0x20; // ' '
    memoria[94] = 0x61; // 'a'
    memoria[95] = 0x20; // ' '
    memoria[96] = 0x57; // 'W'
    memoria[97] = 0x69; // 'i'
    memoria[98] = 0x6E; // 'n'
    memoria[99] = 0x33; // '3'
    memoria[100] = 0x32; // '2'
    memoria[101] = 0x20; // ' '
    memoria[102] = 0x63; // 'c'
    memoria[103] = 0x6F; // 'o'
    memoria[104] = 0x6E; // 'n'
    memoria[105] = 0x73; // 's'
    memoria[106] = 0x6F; // 'o'
    memoria[107] = 0x6C; // 'l'
    memoria[108] = 0x65; // 'e'
    memoria[109] = 0x20; // ' '
    memoria[110] = 0x61; // 'a'
    memoria[111] = 0x70; // 'p'
    memoria[112] = 0x70; // 'p'
    memoria[113] = 0x6C; // 'l'
    memoria[114] = 0x69; // 'i'
    memoria[115] = 0x63; // 'c'
    memoria[116] = 0x61; // 'a'
    memoria[117] = 0x74; // 't'
    memoria[118] = 0x69; // 'i'
    memoria[119] = 0x6F; // 'o'
    memoria[120] = 0x6E; // 'n'
    memoria[121] = 0x2E; // '.'
    memoria[122] = 0x20; // ' '
    memoria[123] = 0x49; // 'I'
    memoria[124] = 0x74; // 't'
    memoria[125] = 0x20; // ' '
    memoria[126] = 0x63; // 'c'
    memoria[127] = 0x61; // 'a'
    memoria[128] = 0x6E; // 'n'
    memoria[129] = 0x6E; // 'n'
    memoria[130] = 0x6F; // 'o'
    memoria[131] = 0x74; // 't'
    memoria[132] = 0x20; // ' '
    memoria[133] = 0x62; // 'b'
    memoria[134] = 0x65; // 'e'
    memoria[135] = 0x20; // ' '
    memoria[136] = 0x72; // 'r'
    memoria[137] = 0x75; // 'u'
    memoria[138] = 0x6E; // 'n'
    memoria[139] = 0x20; // ' '
    memoria[140] = 0x75; // 'u'
    memoria[141] = 0x6E; // 'n'
    memoria[142] = 0x64; // 'd'
    memoria[143] = 0x65; // 'e'
    memoria[144] = 0x72; // 'r'
    memoria[145] = 0x20; // ' '
    memoria[146] = 0x4D; // 'M'
    memoria[147] = 0x53; // 'S'
    memoria[148] = 0x2D; // '-'
    memoria[149] = 0x44; // 'D'
    memoria[150] = 0x4F; // 'O'
    memoria[151] = 0x53; // 'S'
    memoria[152] = 0x2E; // '.'
    memoria[153] = 0x0D; // Carriage return
    memoria[154] = 0x0A; // Line feed
    memoria[155] = 0x24; // String end ('$')
    memoria[156] = 0x00;
    memoria[157] = 0x00;
    memoria[158] = 0x00;
    memoria[159] = 0x00;
    /* COFF HEADER - 8 Standard fields */
    memoria[160] = 0x50; // 'P'
    memoria[161] = 0x45; // 'E'
    memoria[162] = 0x00; // '\0'
    memoria[163] = 0x00; // '\0'
    memoria[164] = 0x4C; // Machine:
    memoria[165] = 0x01; // >= Intel 386
    memoria[166] = 0x01; // Number of
    memoria[167] = 0x00; // sections
    memoria[168] = 0x00; // Time/Date stamp
    memoria[169] = 0x00;
    memoria[170] = 0x53;
    memoria[171] = 0x4C;
    memoria[172] = 0x00; // Pointer to symbol
    memoria[173] = 0x00; // table
    memoria[174] = 0x00; // (deprecated)
    memoria[175] = 0x00;
    memoria[176] = 0x00; // Number of symbols
    memoria[177] = 0x00; // (deprecated)
    memoria[178] = 0x00;
    memoria[179] = 0x00;
    memoria[180] = 0xE0; // Size of optional
    memoria[181] = 0x00; // header
    memoria[182] = 0x02; // Characteristics:
    memoria[183] = 0x01; // 32BIT_MACHINE EXE
    /* OPTIONAL HEADER - 8 Standard fields */
    /* (For image files, it is required) */
    memoria[184] = 0x0B; // Magic number
    memoria[185] = 0x01; // (010B = PE32)
    memoria[186] = 0x01;// Maj.Linker Version
    memoria[187] = 0x00;// Min.Linker Version
    memoria[188] = 0x00; // Size of code
    memoria[189] = 0x06; // (text) section
    memoria[190] = 0x00;
    memoria[191] = 0x00;
    memoria[192] = 0x00; // Size of
    memoria[193] = 0x00; // initialized data
    memoria[194] = 0x00; // section
    memoria[195] = 0x00;
    memoria[196] = 0x00; // Size of
    memoria[197] = 0x00; // uninitialized
    memoria[198] = 0x00; // data section
    memoria[199] = 0x00;
    memoria[200] = 0x00; // Starting address
    memoria[201] = 0x15; // relative to the
    memoria[202] = 0x00; // image base
    memoria[203] = 0x00;
    memoria[204] = 0x00; // Base of code
    memoria[205] = 0x10;
    memoria[206] = 0x00;
    memoria[207] = 0x00;
    /* OPT.HEADER - 1 PE32 specific field */
    memoria[208] = 0x00; // Base of data
    memoria[209] = 0x20;
    memoria[210] = 0x00;
    memoria[211] = 0x00;
    /* OPT.HEADER - 21 Win-Specific Fields */
    memoria[212] = 0x00; // Image base
    memoria[213] = 0x00; // (Preferred
    memoria[214] = 0x40; // address of image
    memoria[215] = 0x00; // when loaded)
    memoria[216] = 0x00; // Section alignment
    memoria[217] = 0x10;
    memoria[218] = 0x00;
    memoria[219] = 0x00;
    memoria[220] = 0x00; // File alignment
    memoria[221] = 0x02; // (Default is 512)
    memoria[222] = 0x00;
    memoria[223] = 0x00;
    memoria[224] = 0x04; // Major OS version
    memoria[225] = 0x00;
    memoria[226] = 0x00; // Minor OS version
    memoria[227] = 0x00;
    memoria[228] = 0x00;// Maj. image version
    memoria[229] = 0x00;
    memoria[230] = 0x00;// Min. image version
    memoria[231] = 0x00;
    memoria[232] = 0x04;// Maj.subsystem ver.
    memoria[233] = 0x00;
    memoria[234] = 0x00;// Min.subsystem ver.
    memoria[235] = 0x00;
    memoria[236] = 0x00; // Win32 version
    memoria[237] = 0x00; // (Reserved, must
    memoria[238] = 0x00; // be zero)
    memoria[239] = 0x00;
    memoria[240] = 0x00;// Size of image
    memoria[241] = 0x20;// (It must be a
    memoria[242] = 0x00;// multiple of the
    memoria[243] = 0x00;// section alignment)
    memoria[244] = 0x00; // Size of headers
    memoria[245] = 0x02; // (rounded up to a
    memoria[246] = 0x00; // multiple of the
    memoria[247] = 0x00; // file alignment)
    memoria[248] = 0x00; // Checksum
    memoria[249] = 0x00;
    memoria[250] = 0x00;
    memoria[251] = 0x00;
    memoria[252] = 0x03; // Windows subsystem
    memoria[253] = 0x00; // (03 = console)
    memoria[254] = 0x00; // DLL characteristics
    memoria[255] = 0x00; // teristics
    memoria[256] = 0x00; // Size of stack
    memoria[257] = 0x00; // reserve
    memoria[258] = 0x10;
    memoria[259] = 0x00;
    memoria[260] = 0x00; // Size of stack
    memoria[261] = 0x10; // commit
    memoria[262] = 0x00;
    memoria[263] = 0x00;
    memoria[264] = 0x00; // Size of heap
    memoria[265] = 0x00; // reserve
    memoria[266] = 0x10;
    memoria[267] = 0x00;
    memoria[268] = 0x00; // Size of heap
    memoria[269] = 0x10; // commit
    memoria[270] = 0x00;
    memoria[271] = 0x00;
    memoria[272] = 0x00; // Loader flags
    memoria[273] = 0x00; // (Reserved, must
    memoria[274] = 0x00; // be zero)
    memoria[275] = 0x00;
    memoria[276] = 0x10; // Number of
    memoria[277] = 0x00; // relative virtual
    memoria[278] = 0x00; // addresses (RVAs)
    memoria[279] = 0x00;
    /* OPT. HEADER - 16 Data Directories */
    memoria[280] = 0x00; // Export Table
    memoria[281] = 0x00;
    memoria[282] = 0x00;
    memoria[283] = 0x00;
    memoria[284] = 0x00;
    memoria[285] = 0x00;
    memoria[286] = 0x00;
    memoria[287] = 0x00;
    memoria[288] = 0x1C; // Import Table
    memoria[289] = 0x10;
    memoria[290] = 0x00;
    memoria[291] = 0x00;
    memoria[292] = 0x28;
    memoria[293] = 0x00;
    memoria[294] = 0x00;
    memoria[295] = 0x00;
    memoria[296] = 0x00; // Resource Table
    memoria[297] = 0x00;
    memoria[298] = 0x00;
    memoria[299] = 0x00;
    memoria[300] = 0x00;
    memoria[301] = 0x00;
    memoria[302] = 0x00;
    memoria[303] = 0x00;
    memoria[304] = 0x00; // Exception Table
    memoria[305] = 0x00;
    memoria[306] = 0x00;
    memoria[307] = 0x00;
    memoria[308] = 0x00;
    memoria[309] = 0x00;
    memoria[310] = 0x00;
    memoria[311] = 0x00;
    memoria[312] = 0x00; // Certificate Table
    memoria[313] = 0x00;
    memoria[314] = 0x00;
    memoria[315] = 0x00;
    memoria[316] = 0x00;
    memoria[317] = 0x00;
    memoria[318] = 0x00;
    memoria[319] = 0x00;
    memoria[320] = 0x00; // Base Relocation
    memoria[321] = 0x00; // Table
    memoria[322] = 0x00;
    memoria[323] = 0x00;
    memoria[324] = 0x00;
    memoria[325] = 0x00;
    memoria[326] = 0x00;
    memoria[327] = 0x00;
    memoria[328] = 0x00; // Debug
    memoria[329] = 0x00;
    memoria[330] = 0x00;
    memoria[331] = 0x00;
    memoria[332] = 0x00;
    memoria[333] = 0x00;
    memoria[334] = 0x00;
    memoria[335] = 0x00;
    memoria[336] = 0x00; // Architecture
    memoria[337] = 0x00;
    memoria[338] = 0x00;
    memoria[339] = 0x00;
    memoria[340] = 0x00;
    memoria[341] = 0x00;
    memoria[342] = 0x00;
    memoria[343] = 0x00;
    memoria[344] = 0x00; // Global Ptr
    memoria[345] = 0x00;
    memoria[346] = 0x00;
    memoria[347] = 0x00;
    memoria[348] = 0x00;
    memoria[349] = 0x00;
    memoria[350] = 0x00;
    memoria[351] = 0x00;
    memoria[352] = 0x00; // TLS Table
    memoria[353] = 0x00;
    memoria[354] = 0x00;
    memoria[355] = 0x00;
    memoria[356] = 0x00;
    memoria[357] = 0x00;
    memoria[358] = 0x00;
    memoria[359] = 0x00;
    memoria[360] = 0x00; // Load Config Table
    memoria[361] = 0x00;
    memoria[362] = 0x00;
    memoria[363] = 0x00;
    memoria[364] = 0x00;
    memoria[365] = 0x00;
    memoria[366] = 0x00;
    memoria[367] = 0x00;
    memoria[368] = 0x00; // Bound Import
    memoria[369] = 0x00;
    memoria[370] = 0x00;
    memoria[371] = 0x00;
    memoria[372] = 0x00;
    memoria[373] = 0x00;
    memoria[374] = 0x00;
    memoria[375] = 0x00;
    memoria[376] = 0x00; // IAT
    memoria[377] = 0x10;
    memoria[378] = 0x00;
    memoria[379] = 0x00;
    memoria[380] = 0x1C;
    memoria[381] = 0x00;
    memoria[382] = 0x00;
    memoria[383] = 0x00;
    memoria[384] = 0x00; // Delay Import
    memoria[385] = 0x00; // Descriptor
    memoria[386] = 0x00;
    memoria[387] = 0x00;
    memoria[388] = 0x00;
    memoria[389] = 0x00;
    memoria[390] = 0x00;
    memoria[391] = 0x00;
    memoria[392] = 0x00; // CLR Runtime
    memoria[393] = 0x00; // Header
    memoria[394] = 0x00;
    memoria[395] = 0x00;
    memoria[396] = 0x00;
    memoria[397] = 0x00;
    memoria[398] = 0x00;
    memoria[399] = 0x00;
    memoria[400] = 0x00; // Reserved, must be
    memoria[401] = 0x00; // zero
    memoria[402] = 0x00;
    memoria[403] = 0x00;
    memoria[404] = 0x00;
    memoria[405] = 0x00;
    memoria[406] = 0x00;
    memoria[407] = 0x00;
    /* SECTIONS TABLE (40 bytes per entry) */
    /* FIRST ENTRY: TEXT HEADER */
    memoria[408] = 0x2E; // '.' (Name)
    memoria[409] = 0x74; // 't'
    memoria[410] = 0x65; // 'e'
    memoria[411] = 0x78; // 'x'
    memoria[412] = 0x74; // 't'
    memoria[413] = 0x00;
    memoria[414] = 0x00;
    memoria[415] = 0x00;
    memoria[416] = 0x24; // Virtual size
    memoria[417] = 0x05;
    memoria[418] = 0x00;
    memoria[419] = 0x00;
    memoria[420] = 0x00; // Virtual address
    memoria[421] = 0x10;
    memoria[422] = 0x00;
    memoria[423] = 0x00;
    memoria[424] = 0x00; // Size of raw data
    memoria[425] = 0x06;
    memoria[426] = 0x00;
    memoria[427] = 0x00;
    memoria[428] = 0x00; // Pointer to
    memoria[429] = 0x02; // raw data
    memoria[430] = 0x00;
    memoria[431] = 0x00;
    memoria[432] = 0x00; // Pointer to
    memoria[433] = 0x00; // relocations
    memoria[434] = 0x00;
    memoria[435] = 0x00;
    memoria[436] = 0x00; // Pointer to
    memoria[437] = 0x00; // line numbers
    memoria[438] = 0x00;
    memoria[439] = 0x00;
    memoria[440] = 0x00; // Number of
    memoria[441] = 0x00; // relocations
    memoria[442] = 0x00; // Number of
    memoria[443] = 0x00; // line numbers
    memoria[444] = 0x20; // Characteristics
    memoria[445] = 0x00; // (Readable,
    memoria[446] = 0x00; // Writeable &
    memoria[447] = 0xE0; // Executable)
    for (i = 448; i < 511; i++) { // 512 = 0x200
        memoria[i] = 0x00;
    }
    memoria[512]=0x6E;
    memoria[513]=0x10;
    memoria[514]=0x0;
    memoria[515]=0x0;
    memoria[516]=0x7C;
    memoria[517]=0x10;
    memoria[518]=0x0;
    memoria[519]=0x0;
    memoria[520]=0x8C;
    memoria[521]=0x10;
    memoria[522]=0x0;
    memoria[523]=0x0;
    memoria[524]=0x98;
    memoria[525]=0x10;
    memoria[526]=0x0;
    memoria[527]=0x0;
    memoria[528]=0xA4;
    memoria[529]=0x10;
    memoria[530]=0x0;
    memoria[531]=0x0;
    memoria[532]=0xB6;
    memoria[533]=0x10;
    memoria[534]=0x0;
    memoria[535]=0x0;
    memoria[536]=0x0;
    memoria[537]=0x0;
    memoria[538]=0x0;
    memoria[539]=0x0;
    memoria[540]=0x52;
    memoria[541]=0x10;
    memoria[542]=0x0;
    memoria[543]=0x0;
    memoria[544]=0x0;
    memoria[545]=0x0;
    memoria[546]=0x0;
    memoria[547]=0x0;
    memoria[548]=0x0;
    memoria[549]=0x0;
    memoria[550]=0x0;
    memoria[551]=0x0;
    memoria[552]=0x44;
    memoria[553]=0x10;
    memoria[554]=0x0;
    memoria[555]=0x0;
    memoria[556]=0x0;
    memoria[557]=0x10;
    memoria[558]=0x0;
    memoria[559]=0x0;
    memoria[560]=0x0;
    memoria[561]=0x0;
    memoria[562]=0x0;
    memoria[563]=0x0;
    memoria[564]=0x0;
    memoria[565]=0x0;
    memoria[566]=0x0;
    memoria[567]=0x0;
    memoria[568]=0x0;
    memoria[569]=0x0;
    memoria[570]=0x0;
    memoria[571]=0x0;
    memoria[572]=0x0;
    memoria[573]=0x0;
    memoria[574]=0x0;
    memoria[575]=0x0;
    memoria[576]=0x0;
    memoria[577]=0x0;
    memoria[578]=0x0;
    memoria[579]=0x0;
    memoria[580]=0x4B;
    memoria[581]=0x45;
    memoria[582]=0x52;
    memoria[583]=0x4E;
    memoria[584]=0x45;
    memoria[585]=0x4C;
    memoria[586]=0x33;
    memoria[587]=0x32;
    memoria[588]=0x2E;
    memoria[589]=0x64;
    memoria[590]=0x6C;
    memoria[591]=0x6C;
    memoria[592]=0x0;
    memoria[593]=0x0;
    memoria[594]=0x6E;
    memoria[595]=0x10;
    memoria[596]=0x0;
    memoria[597]=0x0;
    memoria[598]=0x7C;
    memoria[599]=0x10;
    memoria[600]=0x0;
    memoria[601]=0x0;
    memoria[602]=0x8C;
    memoria[603]=0x10;
    memoria[604]=0x0;
    memoria[605]=0x0;
    memoria[606]=0x98;
    memoria[607]=0x10;
    memoria[608]=0x0;
    memoria[609]=0x0;
    memoria[610]=0xA4;
    memoria[611]=0x10;
    memoria[612]=0x0;
    memoria[613]=0x0;
    memoria[614]=0xB6;
    memoria[615]=0x10;
    memoria[616]=0x0;
    memoria[617]=0x0;
    memoria[618]=0x0;
    memoria[619]=0x0;
    memoria[620]=0x0;
    memoria[621]=0x0;
    memoria[622]=0x0;
    memoria[623]=0x0;
    memoria[624]=0x45;
    memoria[625]=0x78;
    memoria[626]=0x69;
    memoria[627]=0x74;
    memoria[628]=0x50;
    memoria[629]=0x72;
    memoria[630]=0x6F;
    memoria[631]=0x63;
    memoria[632]=0x65;
    memoria[633]=0x73;
    memoria[634]=0x73;
    memoria[635]=0x0;
    memoria[636]=0x0;
    memoria[637]=0x0;
    memoria[638]=0x47;
    memoria[639]=0x65;
    memoria[640]=0x74;
    memoria[641]=0x53;
    memoria[642]=0x74;
    memoria[643]=0x64;
    memoria[644]=0x48;
    memoria[645]=0x61;
    memoria[646]=0x6E;
    memoria[647]=0x64;
    memoria[648]=0x6C;
    memoria[649]=0x65;
    memoria[650]=0x0;
    memoria[651]=0x0;
    memoria[652]=0x0;
    memoria[653]=0x0;
    memoria[654]=0x52;
    memoria[655]=0x65;
    memoria[656]=0x61;
    memoria[657]=0x64;
    memoria[658]=0x46;
    memoria[659]=0x69;
    memoria[660]=0x6C;
    memoria[661]=0x65;
    memoria[662]=0x0;
    memoria[663]=0x0;
    memoria[664]=0x0;
    memoria[665]=0x0;
    memoria[666]=0x57;
    memoria[667]=0x72;
    memoria[668]=0x69;
    memoria[669]=0x74;
    memoria[670]=0x65;
    memoria[671]=0x46;
    memoria[672]=0x69;
    memoria[673]=0x6C;
    memoria[674]=0x65;
    memoria[675]=0x0;
    memoria[676]=0x0;
    memoria[677]=0x0;
    memoria[678]=0x47;
    memoria[679]=0x65;
    memoria[680]=0x74;
    memoria[681]=0x43;
    memoria[682]=0x6F;
    memoria[683]=0x6E;
    memoria[684]=0x73;
    memoria[685]=0x6F;
    memoria[686]=0x6C;
    memoria[687]=0x65;
    memoria[688]=0x4D;
    memoria[689]=0x6F;
    memoria[690]=0x64;
    memoria[691]=0x65;
    memoria[692]=0x0;
    memoria[693]=0x0;
    memoria[694]=0x0;
    memoria[695]=0x0;
    memoria[696]=0x53;
    memoria[697]=0x65;
    memoria[698]=0x74;
    memoria[699]=0x43;
    memoria[700]=0x6F;
    memoria[701]=0x6E;
    memoria[702]=0x73;
    memoria[703]=0x6F;
    memoria[704]=0x6C;
    memoria[705]=0x65;
    memoria[706]=0x4D;
    memoria[707]=0x6F;
    memoria[708]=0x64;
    memoria[709]=0x65;
    memoria[710]=0x0;
    memoria[711]=0x0;
    memoria[712]=0x0;
    memoria[713]=0x0;
    memoria[714]=0x0;
    memoria[715]=0x0;
    memoria[716]=0x0;
    memoria[717]=0x0;
    memoria[718]=0x0;
    memoria[719]=0x0;
    memoria[720]=0x50;
    memoria[721]=0xA2;
    memoria[722]=0x1C;
    memoria[723]=0x11;
    memoria[724]=0x40;
    memoria[725]=0x0;
    memoria[726]=0x31;
    memoria[727]=0xC0;
    memoria[728]=0x3;
    memoria[729]=0x5;
    memoria[730]=0x2C;
    memoria[731]=0x11;
    memoria[732]=0x40;
    memoria[733]=0x0;
    memoria[734]=0x75;
    memoria[735]=0x0D;
    memoria[736]=0x6A;
    memoria[737]=0xF5;
    memoria[738]=0xFF;
    memoria[739]=0x15;
    memoria[740]=0x4;
    memoria[741]=0x10;
    memoria[742]=0x40;
    memoria[743]=0x0;
    memoria[744]=0xA3;
    memoria[745]=0x2C;
    memoria[746]=0x11;
    memoria[747]=0x40;
    memoria[748]=0x0;
    memoria[749]=0x6A;
    memoria[750]=0x0;
    memoria[751]=0x68;
    memoria[752]=0x30;
    memoria[753]=0x11;
    memoria[754]=0x40;
    memoria[755]=0x0;
    memoria[756]=0x6A;
    memoria[757]=0x1;
    memoria[758]=0x68;
    memoria[759]=0x1C;
    memoria[760]=0x11;
    memoria[761]=0x40;
    memoria[762]=0x0;
    memoria[763]=0x50;
    memoria[764]=0xFF;
    memoria[765]=0x15;
    memoria[766]=0x0C;
    memoria[767]=0x10;
    memoria[768]=0x40;
    memoria[769]=0x0;
    memoria[770]=0x9;
    memoria[771]=0xC0;
    memoria[772]=0x75;
    memoria[773]=0x8;
    memoria[774]=0x6A;
    memoria[775]=0x0;
    memoria[776]=0xFF;
    memoria[777]=0x15;
    memoria[778]=0x0;
    memoria[779]=0x10;
    memoria[780]=0x40;
    memoria[781]=0x0;
    memoria[782]=0x81;
    memoria[783]=0x3D;
    memoria[784]=0x30;
    memoria[785]=0x11;
    memoria[786]=0x40;
    memoria[787]=0x0;
    memoria[788]=0x1;
    memoria[789]=0x0;
    memoria[790]=0x0;
    memoria[791]=0x0;
    memoria[792]=0x75;
    memoria[793]=0xEC;
    memoria[794]=0x58;
    memoria[795]=0xC3;
    memoria[796]=0x0;
    memoria[797]=0x57;
    memoria[798]=0x72;
    memoria[799]=0x69;
    memoria[800]=0x74;
    memoria[801]=0x65;
    memoria[802]=0x20;
    memoria[803]=0x65;
    memoria[804]=0x72;
    memoria[805]=0x72;
    memoria[806]=0x6F;
    memoria[807]=0x72;
    memoria[808]=0x0;
    memoria[809]=0x0;
    memoria[810]=0x0;
    memoria[811]=0x0;
    memoria[812]=0x0;
    memoria[813]=0x0;
    memoria[814]=0x0;
    memoria[815]=0x0;
    memoria[816]=0x0;
    memoria[817]=0x0;
    memoria[818]=0x0;
    memoria[819]=0x0;
    memoria[820]=0x0;
    memoria[821]=0x0;
    memoria[822]=0x0;
    memoria[823]=0x0;
    memoria[824]=0x0;
    memoria[825]=0x0;
    memoria[826]=0x0;
    memoria[827]=0x0;
    memoria[828]=0x0;
    memoria[829]=0x0;
    memoria[830]=0x0;
    memoria[831]=0x0;
    memoria[832]=0x60;
    memoria[833]=0x31;
    memoria[834]=0xC0;
    memoria[835]=0x3;
    memoria[836]=0x5;
    memoria[837]=0xCC;
    memoria[838]=0x11;
    memoria[839]=0x40;
    memoria[840]=0x0;
    memoria[841]=0x75;
    memoria[842]=0x37;
    memoria[843]=0x6A;
    memoria[844]=0xF6;
    memoria[845]=0xFF;
    memoria[846]=0x15;
    memoria[847]=0x4;
    memoria[848]=0x10;
    memoria[849]=0x40;
    memoria[850]=0x0;
    memoria[851]=0xA3;
    memoria[852]=0xCC;
    memoria[853]=0x11;
    memoria[854]=0x40;
    memoria[855]=0x0;
    memoria[856]=0x68;
    memoria[857]=0xD0;
    memoria[858]=0x11;
    memoria[859]=0x40;
    memoria[860]=0x0;
    memoria[861]=0x50;
    memoria[862]=0xFF;
    memoria[863]=0x15;
    memoria[864]=0x10;
    memoria[865]=0x10;
    memoria[866]=0x40;
    memoria[867]=0x0;
    memoria[868]=0x80;
    memoria[869]=0x25;
    memoria[870]=0xD0;
    memoria[871]=0x11;
    memoria[872]=0x40;
    memoria[873]=0x0;
    memoria[874]=0xF9;
    memoria[875]=0xFF;
    memoria[876]=0x35;
    memoria[877]=0xD0;
    memoria[878]=0x11;
    memoria[879]=0x40;
    memoria[880]=0x0;
    memoria[881]=0xFF;
    memoria[882]=0x35;
    memoria[883]=0xCC;
    memoria[884]=0x11;
    memoria[885]=0x40;
    memoria[886]=0x0;
    memoria[887]=0xFF;
    memoria[888]=0x15;
    memoria[889]=0x14;
    memoria[890]=0x10;
    memoria[891]=0x40;
    memoria[892]=0x0;
    memoria[893]=0xA1;
    memoria[894]=0xCC;
    memoria[895]=0x11;
    memoria[896]=0x40;
    memoria[897]=0x0;
    memoria[898]=0x6A;
    memoria[899]=0x0;
    memoria[900]=0x68;
    memoria[901]=0xD4;
    memoria[902]=0x11;
    memoria[903]=0x40;
    memoria[904]=0x0;
    memoria[905]=0x6A;
    memoria[906]=0x1;
    memoria[907]=0x68;
    memoria[908]=0xBE;
    memoria[909]=0x11;
    memoria[910]=0x40;
    memoria[911]=0x0;
    memoria[912]=0x50;
    memoria[913]=0xFF;
    memoria[914]=0x15;
    memoria[915]=0x8;
    memoria[916]=0x10;
    memoria[917]=0x40;
    memoria[918]=0x0;
    memoria[919]=0x9;
    memoria[920]=0xC0;
    memoria[921]=0x61;
    memoria[922]=0x90;
    memoria[923]=0x75;
    memoria[924]=0x8;
    memoria[925]=0x6A;
    memoria[926]=0x0;
    memoria[927]=0xFF;
    memoria[928]=0x15;
    memoria[929]=0x0;
    memoria[930]=0x10;
    memoria[931]=0x40;
    memoria[932]=0x0;
    memoria[933]=0x0F;
    memoria[934]=0xB6;
    memoria[935]=0x5;
    memoria[936]=0xBE;
    memoria[937]=0x11;
    memoria[938]=0x40;
    memoria[939]=0x0;
    memoria[940]=0x81;
    memoria[941]=0x3D;
    memoria[942]=0xD4;
    memoria[943]=0x11;
    memoria[944]=0x40;
    memoria[945]=0x0;
    memoria[946]=0x1;
    memoria[947]=0x0;
    memoria[948]=0x0;
    memoria[949]=0x0;
    memoria[950]=0x74;
    memoria[951]=0x5;
    memoria[952]=0xB8;
    memoria[953]=0xFF;
    memoria[954]=0xFF;
    memoria[955]=0xFF;
    memoria[956]=0xFF;
    memoria[957]=0xC3;
    memoria[958]=0x0;
    memoria[959]=0x52;
    memoria[960]=0x65;
    memoria[961]=0x61;
    memoria[962]=0x64;
    memoria[963]=0x20;
    memoria[964]=0x65;
    memoria[965]=0x72;
    memoria[966]=0x72;
    memoria[967]=0x6F;
    memoria[968]=0x72;
    memoria[969]=0x0;
    memoria[970]=0x0;
    memoria[971]=0x0;
    memoria[972]=0x0;
    memoria[973]=0x0;
    memoria[974]=0x0;
    memoria[975]=0x0;
    memoria[976]=0x0;
    memoria[977]=0x0;
    memoria[978]=0x0;
    memoria[979]=0x0;
    memoria[980]=0x0;
    memoria[981]=0x0;
    memoria[982]=0x0;
    memoria[983]=0x0;
    memoria[984]=0x0;
    memoria[985]=0x0;
    memoria[986]=0x0;
    memoria[987]=0x0;
    memoria[988]=0x0;
    memoria[989]=0x0;
    memoria[990]=0x0;
    memoria[991]=0x0;
    memoria[992]=0x60;
    memoria[993]=0x89;
    memoria[994]=0xC6;
    memoria[995]=0x30;
    memoria[996]=0xC0;
    memoria[997]=0x2;
    memoria[998]=0x6;
    memoria[999]=0x74;
    memoria[1000]=0x8;
    memoria[1001]=0x46;
    memoria[1002]=0xE8;
    memoria[1003]=0xE1;
    memoria[1004]=0xFE;
    memoria[1005]=0xFF;
    memoria[1006]=0xFF;
    memoria[1007]=0xEB;
    memoria[1008]=0xF2;
    memoria[1009]=0x61;
    memoria[1010]=0x90;
    memoria[1011]=0xC3;
    memoria[1012]=0x0;
    memoria[1013]=0x0;
    memoria[1014]=0x0;
    memoria[1015]=0x0;
    memoria[1016]=0x0;
    memoria[1017]=0x0;
    memoria[1018]=0x0;
    memoria[1019]=0x0;
    memoria[1020]=0x0;
    memoria[1021]=0x0;
    memoria[1022]=0x0;
    memoria[1023]=0x0;
    memoria[1024]=0x4;
    memoria[1025]=0x30;
    memoria[1026]=0xE8;
    memoria[1027]=0xC9;
    memoria[1028]=0xFE;
    memoria[1029]=0xFF;
    memoria[1030]=0xFF;
    memoria[1031]=0xC3;
    memoria[1032]=0x0;
    memoria[1033]=0x0;
    memoria[1034]=0x0;
    memoria[1035]=0x0;
    memoria[1036]=0x0;
    memoria[1037]=0x0;
    memoria[1038]=0x0;
    memoria[1039]=0x0;
    memoria[1040]=0xB0;
    memoria[1041]=0x0D;
    memoria[1042]=0xE8;
    memoria[1043]=0xB9;
    memoria[1044]=0xFE;
    memoria[1045]=0xFF;
    memoria[1046]=0xFF;
    memoria[1047]=0xB0;
    memoria[1048]=0x0A;
    memoria[1049]=0xE8;
    memoria[1050]=0xB2;
    memoria[1051]=0xFE;
    memoria[1052]=0xFF;
    memoria[1053]=0xFF;
    memoria[1054]=0xC3;
    memoria[1055]=0x0;
    memoria[1056]=0x3D;
    memoria[1057]=0x0;
    memoria[1058]=0x0;
    memoria[1059]=0x0;
    memoria[1060]=0x80;
    memoria[1061]=0x75;
    memoria[1062]=0x4E;
    memoria[1063]=0xB0;
    memoria[1064]=0x2D;
    memoria[1065]=0xE8;
    memoria[1066]=0xA2;
    memoria[1067]=0xFE;
    memoria[1068]=0xFF;
    memoria[1069]=0xFF;
    memoria[1070]=0xB0;
    memoria[1071]=0x2;
    memoria[1072]=0xE8;
    memoria[1073]=0xCB;
    memoria[1074]=0xFF;
    memoria[1075]=0xFF;
    memoria[1076]=0xFF;
    memoria[1077]=0xB0;
    memoria[1078]=0x1;
    memoria[1079]=0xE8;
    memoria[1080]=0xC4;
    memoria[1081]=0xFF;
    memoria[1082]=0xFF;
    memoria[1083]=0xFF;
    memoria[1084]=0xB0;
    memoria[1085]=0x4;
    memoria[1086]=0xE8;
    memoria[1087]=0xBD;
    memoria[1088]=0xFF;
    memoria[1089]=0xFF;
    memoria[1090]=0xFF;
    memoria[1091]=0xB0;
    memoria[1092]=0x7;
    memoria[1093]=0xE8;
    memoria[1094]=0xB6;
    memoria[1095]=0xFF;
    memoria[1096]=0xFF;
    memoria[1097]=0xFF;
    memoria[1098]=0xB0;
    memoria[1099]=0x4;
    memoria[1100]=0xE8;
    memoria[1101]=0xAF;
    memoria[1102]=0xFF;
    memoria[1103]=0xFF;
    memoria[1104]=0xFF;
    memoria[1105]=0xB0;
    memoria[1106]=0x8;
    memoria[1107]=0xE8;
    memoria[1108]=0xA8;
    memoria[1109]=0xFF;
    memoria[1110]=0xFF;
    memoria[1111]=0xFF;
    memoria[1112]=0xB0;
    memoria[1113]=0x3;
    memoria[1114]=0xE8;
    memoria[1115]=0xA1;
    memoria[1116]=0xFF;
    memoria[1117]=0xFF;
    memoria[1118]=0xFF;
    memoria[1119]=0xB0;
    memoria[1120]=0x6;
    memoria[1121]=0xE8;
    memoria[1122]=0x9A;
    memoria[1123]=0xFF;
    memoria[1124]=0xFF;
    memoria[1125]=0xFF;
    memoria[1126]=0xB0;
    memoria[1127]=0x4;
    memoria[1128]=0xE8;
    memoria[1129]=0x93;
    memoria[1130]=0xFF;
    memoria[1131]=0xFF;
    memoria[1132]=0xFF;
    memoria[1133]=0xB0;
    memoria[1134]=0x8;
    memoria[1135]=0xE8;
    memoria[1136]=0x8C;
    memoria[1137]=0xFF;
    memoria[1138]=0xFF;
    memoria[1139]=0xFF;
    memoria[1140]=0xC3;
    memoria[1141]=0x3D;
    memoria[1142]=0x0;
    memoria[1143]=0x0;
    memoria[1144]=0x0;
    memoria[1145]=0x0;
    memoria[1146]=0x7D;
    memoria[1147]=0x0B;
    memoria[1148]=0x50;
    memoria[1149]=0xB0;
    memoria[1150]=0x2D;
    memoria[1151]=0xE8;
    memoria[1152]=0x4C;
    memoria[1153]=0xFE;
    memoria[1154]=0xFF;
    memoria[1155]=0xFF;
    memoria[1156]=0x58;
    memoria[1157]=0xF7;
    memoria[1158]=0xD8;
    memoria[1159]=0x3D;
    memoria[1160]=0x0A;
    memoria[1161]=0x0;
    memoria[1162]=0x0;
    memoria[1163]=0x0;
    memoria[1164]=0x0F;
    memoria[1165]=0x8C;
    memoria[1166]=0xEF;
    memoria[1167]=0x0;
    memoria[1168]=0x0;
    memoria[1169]=0x0;
    memoria[1170]=0x3D;
    memoria[1171]=0x64;
    memoria[1172]=0x0;
    memoria[1173]=0x0;
    memoria[1174]=0x0;
    memoria[1175]=0x0F;
    memoria[1176]=0x8C;
    memoria[1177]=0xD1;
    memoria[1178]=0x0;
    memoria[1179]=0x0;
    memoria[1180]=0x0;
    memoria[1181]=0x3D;
    memoria[1182]=0xE8;
    memoria[1183]=0x3;
    memoria[1184]=0x0;
    memoria[1185]=0x0;
    memoria[1186]=0x0F;
    memoria[1187]=0x8C;
    memoria[1188]=0xB3;
    memoria[1189]=0x0;
    memoria[1190]=0x0;
    memoria[1191]=0x0;
    memoria[1192]=0x3D;
    memoria[1193]=0x10;
    memoria[1194]=0x27;
    memoria[1195]=0x0;
    memoria[1196]=0x0;
    memoria[1197]=0x0F;
    memoria[1198]=0x8C;
    memoria[1199]=0x95;
    memoria[1200]=0x0;
    memoria[1201]=0x0;
    memoria[1202]=0x0;
    memoria[1203]=0x3D;
    memoria[1204]=0xA0;
    memoria[1205]=0x86;
    memoria[1206]=0x1;
    memoria[1207]=0x0;
    memoria[1208]=0x7C;
    memoria[1209]=0x7B;
    memoria[1210]=0x3D;
    memoria[1211]=0x40;
    memoria[1212]=0x42;
    memoria[1213]=0x0F;
    memoria[1214]=0x0;
    memoria[1215]=0x7C;
    memoria[1216]=0x61;
    memoria[1217]=0x3D;
    memoria[1218]=0x80;
    memoria[1219]=0x96;
    memoria[1220]=0x98;
    memoria[1221]=0x0;
    memoria[1222]=0x7C;
    memoria[1223]=0x47;
    memoria[1224]=0x3D;
    memoria[1225]=0x0;
    memoria[1226]=0xE1;
    memoria[1227]=0xF5;
    memoria[1228]=0x5;
    memoria[1229]=0x7C;
    memoria[1230]=0x2D;
    memoria[1231]=0x3D;
    memoria[1232]=0x0;
    memoria[1233]=0xCA;
    memoria[1234]=0x9A;
    memoria[1235]=0x3B;
    memoria[1236]=0x7C;
    memoria[1237]=0x13;
    memoria[1238]=0xBA;
    memoria[1239]=0x0;
    memoria[1240]=0x0;
    memoria[1241]=0x0;
    memoria[1242]=0x0;
    memoria[1243]=0xBB;
    memoria[1244]=0x0;
    memoria[1245]=0xCA;
    memoria[1246]=0x9A;
    memoria[1247]=0x3B;
    memoria[1248]=0xF7;
    memoria[1249]=0xFB;
    memoria[1250]=0x52;
    memoria[1251]=0xE8;
    memoria[1252]=0x18;
    memoria[1253]=0xFF;
    memoria[1254]=0xFF;
    memoria[1255]=0xFF;
    memoria[1256]=0x58;
    memoria[1257]=0xBA;
    memoria[1258]=0x0;
    memoria[1259]=0x0;
    memoria[1260]=0x0;
    memoria[1261]=0x0;
    memoria[1262]=0xBB;
    memoria[1263]=0x0;
    memoria[1264]=0xE1;
    memoria[1265]=0xF5;
    memoria[1266]=0x5;
    memoria[1267]=0xF7;
    memoria[1268]=0xFB;
    memoria[1269]=0x52;
    memoria[1270]=0xE8;
    memoria[1271]=0x5;
    memoria[1272]=0xFF;
    memoria[1273]=0xFF;
    memoria[1274]=0xFF;
    memoria[1275]=0x58;
    memoria[1276]=0xBA;
    memoria[1277]=0x0;
    memoria[1278]=0x0;
    memoria[1279]=0x0;
    memoria[1280]=0x0;
    memoria[1281]=0xBB;
    memoria[1282]=0x80;
    memoria[1283]=0x96;
    memoria[1284]=0x98;
    memoria[1285]=0x0;
    memoria[1286]=0xF7;
    memoria[1287]=0xFB;
    memoria[1288]=0x52;
    memoria[1289]=0xE8;
    memoria[1290]=0xF2;
    memoria[1291]=0xFE;
    memoria[1292]=0xFF;
    memoria[1293]=0xFF;
    memoria[1294]=0x58;
    memoria[1295]=0xBA;
    memoria[1296]=0x0;
    memoria[1297]=0x0;
    memoria[1298]=0x0;
    memoria[1299]=0x0;
    memoria[1300]=0xBB;
    memoria[1301]=0x40;
    memoria[1302]=0x42;
    memoria[1303]=0x0F;
    memoria[1304]=0x0;
    memoria[1305]=0xF7;
    memoria[1306]=0xFB;
    memoria[1307]=0x52;
    memoria[1308]=0xE8;
    memoria[1309]=0xDF;
    memoria[1310]=0xFE;
    memoria[1311]=0xFF;
    memoria[1312]=0xFF;
    memoria[1313]=0x58;
    memoria[1314]=0xBA;
    memoria[1315]=0x0;
    memoria[1316]=0x0;
    memoria[1317]=0x0;
    memoria[1318]=0x0;
    memoria[1319]=0xBB;
    memoria[1320]=0xA0;
    memoria[1321]=0x86;
    memoria[1322]=0x1;
    memoria[1323]=0x0;
    memoria[1324]=0xF7;
    memoria[1325]=0xFB;
    memoria[1326]=0x52;
    memoria[1327]=0xE8;
    memoria[1328]=0xCC;
    memoria[1329]=0xFE;
    memoria[1330]=0xFF;
    memoria[1331]=0xFF;
    memoria[1332]=0x58;
    memoria[1333]=0xBA;
    memoria[1334]=0x0;
    memoria[1335]=0x0;
    memoria[1336]=0x0;
    memoria[1337]=0x0;
    memoria[1338]=0xBB;
    memoria[1339]=0x10;
    memoria[1340]=0x27;
    memoria[1341]=0x0;
    memoria[1342]=0x0;
    memoria[1343]=0xF7;
    memoria[1344]=0xFB;
    memoria[1345]=0x52;
    memoria[1346]=0xE8;
    memoria[1347]=0xB9;
    memoria[1348]=0xFE;
    memoria[1349]=0xFF;
    memoria[1350]=0xFF;
    memoria[1351]=0x58;
    memoria[1352]=0xBA;
    memoria[1353]=0x0;
    memoria[1354]=0x0;
    memoria[1355]=0x0;
    memoria[1356]=0x0;
    memoria[1357]=0xBB;
    memoria[1358]=0xE8;
    memoria[1359]=0x3;
    memoria[1360]=0x0;
    memoria[1361]=0x0;
    memoria[1362]=0xF7;
    memoria[1363]=0xFB;
    memoria[1364]=0x52;
    memoria[1365]=0xE8;
    memoria[1366]=0xA6;
    memoria[1367]=0xFE;
    memoria[1368]=0xFF;
    memoria[1369]=0xFF;
    memoria[1370]=0x58;
    memoria[1371]=0xBA;
    memoria[1372]=0x0;
    memoria[1373]=0x0;
    memoria[1374]=0x0;
    memoria[1375]=0x0;
    memoria[1376]=0xBB;
    memoria[1377]=0x64;
    memoria[1378]=0x0;
    memoria[1379]=0x0;
    memoria[1380]=0x0;
    memoria[1381]=0xF7;
    memoria[1382]=0xFB;
    memoria[1383]=0x52;
    memoria[1384]=0xE8;
    memoria[1385]=0x93;
    memoria[1386]=0xFE;
    memoria[1387]=0xFF;
    memoria[1388]=0xFF;
    memoria[1389]=0x58;
    memoria[1390]=0xBA;
    memoria[1391]=0x0;
    memoria[1392]=0x0;
    memoria[1393]=0x0;
    memoria[1394]=0x0;
    memoria[1395]=0xBB;
    memoria[1396]=0x0A;
    memoria[1397]=0x0;
    memoria[1398]=0x0;
    memoria[1399]=0x0;
    memoria[1400]=0xF7;
    memoria[1401]=0xFB;
    memoria[1402]=0x52;
    memoria[1403]=0xE8;
    memoria[1404]=0x80;
    memoria[1405]=0xFE;
    memoria[1406]=0xFF;
    memoria[1407]=0xFF;
    memoria[1408]=0x58;
    memoria[1409]=0xE8;
    memoria[1410]=0x7A;
    memoria[1411]=0xFE;
    memoria[1412]=0xFF;
    memoria[1413]=0xFF;
    memoria[1414]=0xC3;
    memoria[1415]=0x0;
    memoria[1416]=0xFF;
    memoria[1417]=0x15;
    memoria[1418]=0x0;
    memoria[1419]=0x10;
    memoria[1420]=0x40;
    memoria[1421]=0x0;
    memoria[1422]=0x0;
    memoria[1423]=0x0;
    memoria[1424]=0xB9;
    memoria[1425]=0x0;
    memoria[1426]=0x0;
    memoria[1427]=0x0;
    memoria[1428]=0x0;
    memoria[1429]=0xB3;
    memoria[1430]=0x3;
    memoria[1431]=0x51;
    memoria[1432]=0x53;
    memoria[1433]=0xE8;
    memoria[1434]=0xA2;
    memoria[1435]=0xFD;
    memoria[1436]=0xFF;
    memoria[1437]=0xFF;
    memoria[1438]=0x5B;
    memoria[1439]=0x59;
    memoria[1440]=0x3C;
    memoria[1441]=0x0D;
    memoria[1442]=0x0F;
    memoria[1443]=0x84;
    memoria[1444]=0x34;
    memoria[1445]=0x1;
    memoria[1446]=0x0;
    memoria[1447]=0x0;
    memoria[1448]=0x3C;
    memoria[1449]=0x8;
    memoria[1450]=0x0F;
    memoria[1451]=0x84;
    memoria[1452]=0x94;
    memoria[1453]=0x0;
    memoria[1454]=0x0;
    memoria[1455]=0x0;
    memoria[1456]=0x3C;
    memoria[1457]=0x2D;
    memoria[1458]=0x0F;
    memoria[1459]=0x84;
    memoria[1460]=0x9;
    memoria[1461]=0x1;
    memoria[1462]=0x0;
    memoria[1463]=0x0;
    memoria[1464]=0x3C;
    memoria[1465]=0x30;
    memoria[1466]=0x7C;
    memoria[1467]=0xDB;
    memoria[1468]=0x3C;
    memoria[1469]=0x39;
    memoria[1470]=0x7F;
    memoria[1471]=0xD7;
    memoria[1472]=0x2C;
    memoria[1473]=0x30;
    memoria[1474]=0x80;
    memoria[1475]=0xFB;
    memoria[1476]=0x0;
    memoria[1477]=0x74;
    memoria[1478]=0xD0;
    memoria[1479]=0x80;
    memoria[1480]=0xFB;
    memoria[1481]=0x2;
    memoria[1482]=0x75;
    memoria[1483]=0x0C;
    memoria[1484]=0x81;
    memoria[1485]=0xF9;
    memoria[1486]=0x0;
    memoria[1487]=0x0;
    memoria[1488]=0x0;
    memoria[1489]=0x0;
    memoria[1490]=0x75;
    memoria[1491]=0x4;
    memoria[1492]=0x3C;
    memoria[1493]=0x0;
    memoria[1494]=0x74;
    memoria[1495]=0xBF;
    memoria[1496]=0x80;
    memoria[1497]=0xFB;
    memoria[1498]=0x3;
    memoria[1499]=0x75;
    memoria[1500]=0x0A;
    memoria[1501]=0x3C;
    memoria[1502]=0x0;
    memoria[1503]=0x75;
    memoria[1504]=0x4;
    memoria[1505]=0xB3;
    memoria[1506]=0x0;
    memoria[1507]=0xEB;
    memoria[1508]=0x2;
    memoria[1509]=0xB3;
    memoria[1510]=0x1;
    memoria[1511]=0x81;
    memoria[1512]=0xF9;
    memoria[1513]=0xCC;
    memoria[1514]=0xCC;
    memoria[1515]=0xCC;
    memoria[1516]=0x0C;
    memoria[1517]=0x7F;
    memoria[1518]=0xA8;
    memoria[1519]=0x81;
    memoria[1520]=0xF9;
    memoria[1521]=0x34;
    memoria[1522]=0x33;
    memoria[1523]=0x33;
    memoria[1524]=0xF3;
    memoria[1525]=0x7C;
    memoria[1526]=0xA0;
    memoria[1527]=0x88;
    memoria[1528]=0xC7;
    memoria[1529]=0xB8;
    memoria[1530]=0x0A;
    memoria[1531]=0x0;
    memoria[1532]=0x0;
    memoria[1533]=0x0;
    memoria[1534]=0xF7;
    memoria[1535]=0xE9;
    memoria[1536]=0x3D;
    memoria[1537]=0x8;
    memoria[1538]=0x0;
    memoria[1539]=0x0;
    memoria[1540]=0x80;
    memoria[1541]=0x74;
    memoria[1542]=0x11;
    memoria[1543]=0x3D;
    memoria[1544]=0xF8;
    memoria[1545]=0xFF;
    memoria[1546]=0xFF;
    memoria[1547]=0x7F;
    memoria[1548]=0x75;
    memoria[1549]=0x13;
    memoria[1550]=0x80;
    memoria[1551]=0xFF;
    memoria[1552]=0x7;
    memoria[1553]=0x7E;
    memoria[1554]=0x0E;
    memoria[1555]=0xE9;
    memoria[1556]=0x7F;
    memoria[1557]=0xFF;
    memoria[1558]=0xFF;
    memoria[1559]=0xFF;
    memoria[1560]=0x80;
    memoria[1561]=0xFF;
    memoria[1562]=0x8;
    memoria[1563]=0x0F;
    memoria[1564]=0x8F;
    memoria[1565]=0x76;
    memoria[1566]=0xFF;
    memoria[1567]=0xFF;
    memoria[1568]=0xFF;
    memoria[1569]=0xB9;
    memoria[1570]=0x0;
    memoria[1571]=0x0;
    memoria[1572]=0x0;
    memoria[1573]=0x0;
    memoria[1574]=0x88;
    memoria[1575]=0xF9;
    memoria[1576]=0x80;
    memoria[1577]=0xFB;
    memoria[1578]=0x2;
    memoria[1579]=0x74;
    memoria[1580]=0x4;
    memoria[1581]=0x1;
    memoria[1582]=0xC1;
    memoria[1583]=0xEB;
    memoria[1584]=0x3;
    memoria[1585]=0x29;
    memoria[1586]=0xC8;
    memoria[1587]=0x91;
    memoria[1588]=0x88;
    memoria[1589]=0xF8;
    memoria[1590]=0x51;
    memoria[1591]=0x53;
    memoria[1592]=0xE8;
    memoria[1593]=0xC3;
    memoria[1594]=0xFD;
    memoria[1595]=0xFF;
    memoria[1596]=0xFF;
    memoria[1597]=0x5B;
    memoria[1598]=0x59;
    memoria[1599]=0xE9;
    memoria[1600]=0x53;
    memoria[1601]=0xFF;
    memoria[1602]=0xFF;
    memoria[1603]=0xFF;
    memoria[1604]=0x80;
    memoria[1605]=0xFB;
    memoria[1606]=0x3;
    memoria[1607]=0x0F;
    memoria[1608]=0x84;
    memoria[1609]=0x4A;
    memoria[1610]=0xFF;
    memoria[1611]=0xFF;
    memoria[1612]=0xFF;
    memoria[1613]=0x51;
    memoria[1614]=0x53;
    memoria[1615]=0xB0;
    memoria[1616]=0x8;
    memoria[1617]=0xE8;
    memoria[1618]=0x7A;
    memoria[1619]=0xFC;
    memoria[1620]=0xFF;
    memoria[1621]=0xFF;
    memoria[1622]=0xB0;
    memoria[1623]=0x20;
    memoria[1624]=0xE8;
    memoria[1625]=0x73;
    memoria[1626]=0xFC;
    memoria[1627]=0xFF;
    memoria[1628]=0xFF;
    memoria[1629]=0xB0;
    memoria[1630]=0x8;
    memoria[1631]=0xE8;
    memoria[1632]=0x6C;
    memoria[1633]=0xFC;
    memoria[1634]=0xFF;
    memoria[1635]=0xFF;
    memoria[1636]=0x5B;
    memoria[1637]=0x59;
    memoria[1638]=0x80;
    memoria[1639]=0xFB;
    memoria[1640]=0x0;
    memoria[1641]=0x75;
    memoria[1642]=0x7;
    memoria[1643]=0xB3;
    memoria[1644]=0x3;
    memoria[1645]=0xE9;
    memoria[1646]=0x25;
    memoria[1647]=0xFF;
    memoria[1648]=0xFF;
    memoria[1649]=0xFF;
    memoria[1650]=0x80;
    memoria[1651]=0xFB;
    memoria[1652]=0x2;
    memoria[1653]=0x75;
    memoria[1654]=0x0F;
    memoria[1655]=0x81;
    memoria[1656]=0xF9;
    memoria[1657]=0x0;
    memoria[1658]=0x0;
    memoria[1659]=0x0;
    memoria[1660]=0x0;
    memoria[1661]=0x75;
    memoria[1662]=0x7;
    memoria[1663]=0xB3;
    memoria[1664]=0x3;
    memoria[1665]=0xE9;
    memoria[1666]=0x11;
    memoria[1667]=0xFF;
    memoria[1668]=0xFF;
    memoria[1669]=0xFF;
    memoria[1670]=0x89;
    memoria[1671]=0xC8;
    memoria[1672]=0xB9;
    memoria[1673]=0x0A;
    memoria[1674]=0x0;
    memoria[1675]=0x0;
    memoria[1676]=0x0;
    memoria[1677]=0xBA;
    memoria[1678]=0x0;
    memoria[1679]=0x0;
    memoria[1680]=0x0;
    memoria[1681]=0x0;
    memoria[1682]=0x3D;
    memoria[1683]=0x0;
    memoria[1684]=0x0;
    memoria[1685]=0x0;
    memoria[1686]=0x0;
    memoria[1687]=0x7D;
    memoria[1688]=0x8;
    memoria[1689]=0xF7;
    memoria[1690]=0xD8;
    memoria[1691]=0xF7;
    memoria[1692]=0xF9;
    memoria[1693]=0xF7;
    memoria[1694]=0xD8;
    memoria[1695]=0xEB;
    memoria[1696]=0x2;
    memoria[1697]=0xF7;
    memoria[1698]=0xF9;
    memoria[1699]=0x89;
    memoria[1700]=0xC1;
    memoria[1701]=0x81;
    memoria[1702]=0xF9;
    memoria[1703]=0x0;
    memoria[1704]=0x0;
    memoria[1705]=0x0;
    memoria[1706]=0x0;
    memoria[1707]=0x0F;
    memoria[1708]=0x85;
    memoria[1709]=0xE6;
    memoria[1710]=0xFE;
    memoria[1711]=0xFF;
    memoria[1712]=0xFF;
    memoria[1713]=0x80;
    memoria[1714]=0xFB;
    memoria[1715]=0x2;
    memoria[1716]=0x0F;
    memoria[1717]=0x84;
    memoria[1718]=0xDD;
    memoria[1719]=0xFE;
    memoria[1720]=0xFF;
    memoria[1721]=0xFF;
    memoria[1722]=0xB3;
    memoria[1723]=0x3;
    memoria[1724]=0xE9;
    memoria[1725]=0xD6;
    memoria[1726]=0xFE;
    memoria[1727]=0xFF;
    memoria[1728]=0xFF;
    memoria[1729]=0x80;
    memoria[1730]=0xFB;
    memoria[1731]=0x3;
    memoria[1732]=0x0F;
    memoria[1733]=0x85;
    memoria[1734]=0xCD;
    memoria[1735]=0xFE;
    memoria[1736]=0xFF;
    memoria[1737]=0xFF;
    memoria[1738]=0xB0;
    memoria[1739]=0x2D;
    memoria[1740]=0x51;
    memoria[1741]=0x53;
    memoria[1742]=0xE8;
    memoria[1743]=0xFD;
    memoria[1744]=0xFB;
    memoria[1745]=0xFF;
    memoria[1746]=0xFF;
    memoria[1747]=0x5B;
    memoria[1748]=0x59;
    memoria[1749]=0xB3;
    memoria[1750]=0x2;
    memoria[1751]=0xE9;
    memoria[1752]=0xBB;
    memoria[1753]=0xFE;
    memoria[1754]=0xFF;
    memoria[1755]=0xFF;
    memoria[1756]=0x80;
    memoria[1757]=0xFB;
    memoria[1758]=0x3;
    memoria[1759]=0x0F;
    memoria[1760]=0x84;
    memoria[1761]=0xB2;
    memoria[1762]=0xFE;
    memoria[1763]=0xFF;
    memoria[1764]=0xFF;
    memoria[1765]=0x80;
    memoria[1766]=0xFB;
    memoria[1767]=0x2;
    memoria[1768]=0x75;
    memoria[1769]=0x0C;
    memoria[1770]=0x81;
    memoria[1771]=0xF9;
    memoria[1772]=0x0;
    memoria[1773]=0x0;
    memoria[1774]=0x0;
    memoria[1775]=0x0;
    memoria[1776]=0x0F;
    memoria[1777]=0x84;
    memoria[1778]=0xA1;
    memoria[1779]=0xFE;
    memoria[1780]=0xFF;
    memoria[1781]=0xFF;
    memoria[1782]=0x51;
    memoria[1783]=0xE8;
    memoria[1784]=0x14;
    memoria[1785]=0xFD;
    memoria[1786]=0xFF;
    memoria[1787]=0xFF;
    memoria[1788]=0x59;
    memoria[1789]=0x89;
    memoria[1790]=0xC8;
    memoria[1791]=0xC3;
    posVec = 1792;
}
