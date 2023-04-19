#!/usr/bin/perl

# Copyright (c) 2023 Todd T. Fries <todd@fries.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;

use Data::Dumper;
use Getopt::Std;
use Gov::Data;
use ReadConf;
use Date::Manip;

# definitions
our $opt_b; # look up a single bill
our $opt_c; # config file override
our $opt_t; # bill type
our $opt_o; # if set, dir to save various versions of bill
our $opt_v; # verbose+++

# defaults
$opt_c = $ENV{HOME}."/.config/govdata/govdata.conf";
$opt_v = 0;

getopts('b:c:o:t:v');

our $verbose = $opt_v;
our $rlim = 0;
our $rlim_remain = 0;

my $config_file = $opt_c;

my $r = ReadConf->new();

my $config = $r->readconf( $config_file );

our $api_key = $config->{_}->{key};
my $base_url = 'https://api.congress.gov/v3';
our $maxc = $config->{_}->{max_congress_seen};
if (!defined($maxc)) {
	$maxc = 0;
}

my $info = {
	'ApiKey' => $api_key,
	'format' => 'json',
	'UserAgent' => 'perl c(ongress)/0.1',
	'verbose' => $verbose,
};

our $gd = Gov::Data->new($info);

# some defaults to aid in parsing
our @billtypes = ("H", "HR", "S", "HJRES", "SJRES", "HCONRES", "SCONRES", "HRES", "SRES");

if (defined($opt_t)) {
	my $ut = uc($opt_t);
	my @bmatch = grep {$ut eq $_} @billtypes;
	if (!@bmatch) {
		die "not a valid bill type: $ut";
	}
}

if (defined($opt_b)) {
	my @tlist = @billtypes;
	if (defined($opt_t)) {
		@tlist = ( uc($opt_t) );
	}
	my $found = 0;
	my $myc = $maxc;
	while ($myc > 0) {
	    foreach my $ut (@tlist) {
		my $url = "$base_url/bill/$myc/${ut}/${opt_b}";
		my $bdata = $gd->get_json( $url );
		if (!defined($bdata)) {
			next;
		}
		$found++;
		fmt_bill( $bdata->{bill} );
		last;
	    }
	    if ($found > 0) {
		last;
	    }
	    print " no bills matching ${opt_b} in congress#${myc}\n";
	    $myc--;
	    show_limits();
	    sleep(1);
	}
	exit(0);
}


# Default request to retrieve a list of bills sorted by date of latest action
my $url = "$base_url/bill";

my $data = $gd->get_json( $url );

foreach my $bill ( @{ $data->{bills} } ) {

	my $bdata = $gd->get_json( $bill->{url} );
	fmt_bill($data->{bill});

}

sub fmt_actions {
	my ($actions) = @_;
	my $adate = Date::Manip::Date->new();
	foreach my $a (@{ $actions }) {
		print " ";
		my $dstr = $a->{actionDate};
		if (defined($a->{actionTime})) {
			$dstr .= " ".$a->{actionTime}
		}
		$adate->parse($dstr);
		print $adate->printf("%Y%m%d %H:%M")." ";
		if (defined($a->{actionCode})) {
			print $a->{actionCode}." ";
		}
		print $a->{type}." ";
		print $a->{text}."\n";
		foreach my $vote (@{$a->{recordedVotes}}) {
			print " ";
			print $vote->{date}." ";
			my $data = $gd->get_xml($vote->{url});
			if (defined($data->{count}->{yeas})) {
				print " Yeas/Nays = ".$data->{count}->{yeas}."/".$data->{count}->{nays}."\n";
			} elsif (defined($data->{'vote-metadata'}->{'vote-question'})) {
				my $vmd = $data->{'vote-metadata'};
				print " Question: ";
				print $vmd->{'vote-question'}." ";
				print " Yeas/Nays/Abstain = ";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'yea-total'}."/";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'nay-total'}."/";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'not-voting-total'}."\n";
			} else {
				print Dumper($data)."\n";
			}
		}
	}
}

# format sponsors
sub fmt_sponsors {
	my ($slist) = @_;

	my $i = 0;
	foreach my $s ( sort { $a->{lastName} cmp $b->{lastName} } @{ $slist } ) {
		print " ";
		print $s->{state}." ";
		print $s->{party}." ";
		print $s->{lastName}.", ";
		print $s->{firstName}."\n";
		$i++;
	}
	if ($i < 1) {
		print "\n";
	}
}

# format amendments
sub fmt_amendments {
	my ($adata) = @_;
	my $udate = Date::Manip::Date->new();
	my $adate = Date::Manip::Date->new();

	my $i = 0;
	foreach my $a ( sort { $a->{number} >= $b->{number} } @{ $adata } ) {
		$udate->parse( $a->{updateDate} );
		$adate->parse( $a->{latestAction}->{actionDate}."T".
			$a->{latestAction}->{actionTime} );
		print " ";
		print $a->{number}." ";
		print $a->{type}." ";
		print $a->{congress}." ";
		print $udate->printf("%Y%m%d %H:%M")." ";
		print $adate->printf("%Y%m%d %H:%M")." ";
		print $a->{description};
		print "\n";
		printf "%45s", " ";
		print $a->{url};
		print "\n";
		my $amdata = $gd->get_json( $a->{url} );
		fmt_amendment_url( $amdata->{amendment} );
	}
}

# format amendment urls
sub fmt_amendment_url {
	my ($adata) = @_;
	my $congress = $adata->{congress};
	my $billtype = $adata->{amendedBill}->{originChamberCode};
	my $billno   = $adata->{amendedBill}->{number};
	my $ano      = $adata->{number};
	my $atype    = lc($adata->{type});

	my $actions = $adata->{actions};
	my $actcount = $actions->{count};
	if (defined($actcount) && $actcount > 0) {
		print "Actions: ${actcount}\n";
		my $actdata = $gd->get_json( $actions->{url} );
		fmt_actions( $actdata->{actions} );
	} else {
		print "Actions: 0\n";
	}
	print "Sponsor: ";
	fmt_sponsors( $adata->{sponsors} );
	#print "Text: ";
	#my $url = "https://api.congress.gov/v3/amendment/${congress}/${atype}/${ano}";
	#my $atxt = $gd->get_json( $url );
	#print $atxt."\n";
	exit(0);
}

# format bill
sub fmt_bill {
	my ($bill) = @_;

	# looks like originChamberCode not set for bill/<c>/<type>/<no>
	# but is set for bill api calls
	if (!defined($bill->{originChamberCode})) {
		if (!defined($bill->{originChamber})) {
			$bill->{originChamberCode} = "U"; # unknown
		} else {
			if ($bill->{originChamber} eq "Senate") {
				$bill->{originChamberCode} = "S";
			} elsif ($bill->{originChamber} eq "House") {
				$bill->{originChamberCode} = "H";
			} else {
				$bill->{originChamberCode} = "U";
			}
		}
	}

	print "Bill Title: $bill->{title}\n";
	print "Bill Info: ";
	print "C".$bill->{congress}." ";
	update_maxc( $bill->{congress} );
	print $bill->{originChamberCode}." ";
	print $bill->{type}." ";
	print $bill->{number}."\n";
	print "Latest Action: ".$bill->{latestAction}->{actionDate}." ";
	print $bill->{latestAction}->{text}."\n";

	if (defined($opt_o) && -d ${opt_o}) {
		my $congress = $bill->{congress};
		my $oc = lc($bill->{originChamberCode});
		my $bno = $bill->{number};
		my $url = "https://api.congress.gov/v3/bill/${congress}/${oc}/${bno}/text";
		$url = $bill->{textVersions}->{url};
		my $tinfo = $gd->get_json( $url );
		fmt_text( $tinfo );
	}


	print "Sponsor: ";
	fmt_sponsors( $bill->{sponsors} );

	my $amendments = $bill->{amendments};
	my $acount = $amendments->{count};
	if (defined($acount) && $acount > 0) {
		print "Amendments: ${acount}\n";
		$url = $bill->{amendments}->{url};
		my $adata = $gd->get_json( $url );
		fmt_amendments( $adata->{amendments} );
	} else {
		print "Amendments: 0\n";
	}

	my $cosponsors = $bill->{cosponsors};
	my $cocount = $cosponsors->{count};
	if (defined($cocount) && $cocount > 0) {
		print "Cosponsors: ${cocount}\n";
		$url = $cosponsors->{url};
		my $codata = $gd->get_json( $url );
		fmt_sponsors( $codata->{cosponsors} );
	} else {
		print "Cosponsors: 0\n";
	}

	my $actions = $bill->{actions};
	my $actcount = $actions->{count};
	if (defined($actcount) && $actcount > 0) {
		print "Actions: ${actcount}\n";
		$verbose++;
		my $adata = $gd->get_json( $actions->{url} );
		$verbose--;
		fmt_actions( $adata->{actions} );
	} else {
		print "Actions: 0\n";
	}
	print "\n";
}

sub fmt_text {
	my ($t) = @_;
	#print "fmt_text debug: ".Dumper($t);
	my $congress = $t->{request}->{congress};
	my $bno = $t->{request}->{billNumber};
	my $bt = $t->{request}->{billType};

	foreach my $f ( @{$t->{textVersions}} ) {
		if (!defined($f->{type})) {
			next;
		}
		printf "text: congress %s bill %s type %s ttype %s\n\n",
			$congress,
			$bno,
			$bt,
			$f->{type};
		foreach my $format (@{$f->{formats}}) {
			my $ext;
			print "fmt_text: debug format->type = ".
				$format->{type}."\n";
			print "fmt_text: debug format->url = ".
				$format->{url}."\n";
			if ($format->{type} eq "Formatted Text") {
				$ext = "txt";
			} elsif ($format->{type} eq "PDF") {
				$ext = "pdf";
			} elsif ($format->{type} eq "Formatted XML") {
				$ext = "xml";
			} else {
				print "fmt_text unhandled type: ";
				print $f->{type}."\n";
				next;
			}
			my $data = $gd->get_info( $format->{url} );
			my $name = sprintf "%s/%s-%s-%s-%s.%s",
				$opt_o, $congress, $bno, $bt, $f->{type}, $ext;
			$name =~ s/\s/_/g;
			open(T, ">", $name);
			print T $data;
			close(T);
		}
	}
}

# error check?
sub update_maxc {
	my ($c) = @_;

	if ($c <= $maxc) {
		return;
	}
	my $tmpfile = $opt_c.".tmp";

	$maxc = $c;
	open(C, "<", $opt_c);
	open(N, ">", $tmpfile);
	my $mcfound = 0;
	while (<C>) {
		if (/^maxc/) {
			print N "max_congress_seen = ${maxc}\n";
			next;
		}
		print N $_;
	}
	close(C);
	if ($mcfound < 1) {
		print N "max_congress_seen = ${maxc}\n";
	}
	close(N);
	rename($opt_c.".tmp",$opt_c);
}

sub show_limits {
	printf "   %s/%s left\n", $rlim_remain, $rlim;
}
