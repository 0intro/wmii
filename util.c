
int
max(int a, int b) {
	if(a > b)
		return a;
	return b;
}

char *
str_nil(char *s) {
	if(s)
		return s;
	return "<nil>";
}
