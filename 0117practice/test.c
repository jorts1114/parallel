#include <stdio.h>

int main() {

  int is=10;
  #pragma omp parallel for reduction(+:is)
    for(int j=1; j<=10; j++) {
      is += j;
  }
  printf("%d\n", is);
  return 0;
}
    
