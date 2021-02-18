#include <sanitizer/dfsan_interface.h>
#include <stdio.h>
#include <assert.h>

int main(void) {
   
  int x, y, z, loop;
  x = 1;

  dfsan_label x_label = dfsan_create_label("x");
  dfsan_set_label(x_label, &x, sizeof(x));

  if (x > 0) { 
    y = 4 * x;
    z = y % 4;

    loop = y;
    for (int i=1; i<5; i++) {
      loop = loop * i;
    }
  }

  /*dfsan_label x_label = dfsan_get_label(x);*/
  dfsan_label y_label = dfsan_get_label(y);
  dfsan_label z_label = dfsan_get_label(z);
  dfsan_label loop_label = dfsan_get_label(loop);

  const struct dfsan_label_info* x_info = dfsan_get_label_info(x_label);
  const struct dfsan_label_info* y_info = dfsan_get_label_info(y_label);
  const struct dfsan_label_info* z_info = dfsan_get_label_info(z_label);
  const struct dfsan_label_info* loop_info = dfsan_get_label_info(loop_label);

  printf("x     label %d: %f, %f\n",x_label, x_info->neg_dydx, x_info->pos_dydx);
  printf("y=x*4 label %d: %f, %f\n",y_label, y_info->neg_dydx, y_info->pos_dydx);
  printf("z=y(mod)4 label %d: %f, %f\n",z_label, z_info->neg_dydx, z_info->pos_dydx);
  printf("loop  label %d: %f, %f\n",loop_label, loop_info->neg_dydx, loop_info->pos_dydx);

  return 0;
}
