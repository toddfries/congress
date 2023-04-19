# congress

Last year api.congress.gov was born.

This year, lets use it to track our congressional sentors and representatives.

Iterate forward!

Requirements:

```
	use Data::Dumper;
	use Getopt::Std;
	use ReadConf;
	use Gov::Data; # see github.com/toddfries/govapi
```

Thusfar, `c` can list recently actioned bills and iterate over bits of them.

It can also retrieve info about just one.

Usage:
```
	perl ./c | less # list of recent bills
	mkdir -p s686
	# details about Senate bill 686, save documents in 's686' dir
	perl ./c -b 686 -t s -o s686
```
