struct S { int a, b, c; };
struct S g;
int
main(void)
{
	struct S x = {0};
	struct S y;
	y = x;
	g = y;
	return g.a;
}
