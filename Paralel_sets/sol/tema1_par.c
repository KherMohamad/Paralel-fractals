#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;

// structura pentru un numar complex
typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia, iterations;
	double x_min, x_max, y_min, y_max, resolution;
	complex c_julia;
} params;

int widthJ, heightJ; //marimile matricei Julia
int widthM, heightM; //marimile matricei Mandelbrot
int **resultJ;       //matricea Julia
int **resultM;       //matricea Mandelbrot
params parJ;         //parametrii de intrare ai multimii Julia
params parM;         //parametrii de intrare ai multimii Mandelbrot
int P;               //numarul de Threaduri
pthread_barrier_t bar;   //bariera folosita in thread function

// citeste argumentele programului
void get_args(int argc, char **argv)
{
	if (argc < 5) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1 fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
	P = atoi(argv[5]);
}

void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);
	fscanf(file, "%lf %lf %lf %lf",
			&par->x_min, &par->x_max, &par->y_min, &par->y_max);
	fscanf(file, "%lf", &par->resolution);
	fscanf(file, "%d", &par->iterations);

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

void write_output_file(char *out_filename, int **result, int width, int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}

	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

int **allocate_memory(int width, int height)
{
	int **result;
	int i;

	result = malloc(height * sizeof(int*));
	if (result == NULL) {
		printf("Eroare la malloc!\n");
		exit(1);
	}

	for (i = 0; i < height; i++) {
		result[i] = malloc(width * sizeof(int));
		if (result[i] == NULL) {
			printf("Eroare la malloc!\n");
			exit(1);
		}
	}

	return result;
}

void free_memory(int **result, int height)
{
	int i;

	for (i = 0; i < height; i++) {
		free(result[i]);
	}
	free(result);
}

void run_julia(params *par, int **result, int start, int end)
{
	int w, h;

	for (w = 0; w < widthJ; w++) {
		for (h = start; h < end; h++) {             //for paralelizat
			int step = 0;
			complex z = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + par->c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + par->c_julia.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}


}

void run_mandelbrot(params *par, int **result, int start, int end)
{
	int w, h;

	for (w = 0; w < widthM; w++) {
		for (h = start; h < end; h++) {                            //for paralelizat
			complex c = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}


}
void transformCoordinates(int **result, int start, int end, int height) {        //am implementat transformarea intr-o functie separata
	int i;                                                                       // pentru a usura apelarea barierei in threadFunction

	for (i = start / 2; i < end / 2; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}
}

int min(int a, int b) {
	return !(b<a)?a:b;  
}
void *threadFunction(void *arg) {
	int id = *(int *)arg;
	int startJ = id * (double)heightJ / P;               //indicii de inceput si sfarsit ai partitiei
	int startM = id * (double)heightM / P;
	int endJ = min((id + 1) * (double) heightJ / P, heightJ);
	int endM = min((id + 1) * (double) heightM / P, heightM);
	run_julia(&parJ, resultJ, startJ, endJ);              //aplica julia =>asteapta terminarea pe toate threadurile => transforma in coordonate ecran
	pthread_barrier_wait(&bar);
	transformCoordinates(resultJ, startJ, endJ, heightJ);
	run_mandelbrot(&parM, resultM, startM, endM);        //aplica Mandelbrot =>asteapta terminarea pe toate threadurile => transforma in coordonate ecran
	pthread_barrier_wait(&bar);
	transformCoordinates(resultM, startM, endM, heightM);
	
}

int main(int argc, char* argv[]) {
	pthread_t threads[8];
	get_args(argc, argv);
	int ret;
    int args[8];


	//se citesc parametrii de intrare si se calculeaza marimiile matricelor
	read_input_file(in_filename_julia, &parJ);
	read_input_file(in_filename_mandelbrot, &parM);

	widthJ = (parJ.x_max - parJ.x_min) / parJ.resolution;
	heightJ = (parJ.y_max - parJ.y_min) / parJ.resolution;
	
	widthM = (parM.x_max - parM.x_min) / parM.resolution;
	heightM = (parM.y_max - parM.y_min) / parM.resolution;
	
	resultJ = allocate_memory(widthJ, heightJ);
	resultM = allocate_memory(widthM, heightM);

	pthread_barrier_init(&bar, NULL, P);


	//creearea si joinul firelor de executie
	for (int i = 0; i < P; i++) {
		args[i] = i;
		ret = pthread_create(&threads[i], NULL, threadFunction, (void *) &args[i]);
		
		if (ret) {
			printf("error creating thread %d\n", args[i]);
			exit(-1);
		}
	}
	for (int i = 0; i < P; i++) {
		ret = pthread_join(threads[i], NULL);
		
		if (ret) {
			printf("error joining thread %d\n", args[i]);
		}
	}

	//se scriu matricele rezultate in fisierele de iesire si se elibereaza memoria
	write_output_file(out_filename_julia, resultJ, widthJ, heightJ);
	write_output_file(out_filename_mandelbrot, resultM, widthM, heightM);

	pthread_barrier_destroy(&bar);
	
	free_memory(resultJ, heightJ);
	free_memory(resultM, heightM);
	
	return 0;
}
		
	
	
	
	
	

