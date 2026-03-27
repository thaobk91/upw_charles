#include "algorithm.h"
#include "mocked_app.h"

static volatile bool flag = false;

void mocked_app_check_in( void )
{
  if (flag)
  {
    flag = false;
    Algorithm.Check_In();
  }
}

void mocked_app_set_flag( void )
{
  flag = true;
}