struct S { int a, b, c; };
struct S g;
int
main(void)
{
	g = (struct S){0};
	return g.a;
}
