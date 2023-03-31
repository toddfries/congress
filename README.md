# congress

Last year api.congress.gov was born.

This year, lets use it to track our congressional sentors and representatives.

Iterate forward!

Requirements:

```
	use Data::Dumper;
	use Getopt::Std;
	use JSON qw( decode_json );
	use LWP::UserAgent;
	use ReadConf;
	use XML::Simple;
```

Thusfar, `c` can list recently actioned bills and iterate over bits of them.

It can also retrieve info about just one.

Usage:
```
	perl ./c | less # list of recent bills
	perl ./c -b 868 -t s # details about Senate bill 868
```
