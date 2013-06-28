BEGIN{
	for(i=0; i < 256; i++)
	{
		hex2c[sprintf("%02x", i)]=sprintf("%c", i)
		hex2c[sprintf("%02X", i)]=sprintf("%c", i)
		hex2c[sprintf("%x", i)]=sprintf("%c", i)
		hex2c[sprintf("%X", i)]=sprintf("%c", i)

		hex2n[sprintf("%02x", i)]=sprintf("%i", i)
		hex2n[sprintf("%02X", i)]=sprintf("%i", i)
		hex2n[sprintf("%x", i)]=sprintf("%i", i)
		hex2n[sprintf("%X", i)]=sprintf("%i", i)
	}
}

{
	for(i=1; i <= NF; i++)
		if($i in hex2c)
			printf("%s", hex2c[$i])
		else
			printf("--%s--", $i)
	
	print ""
	for(i=1; i <= NF; i++)
		if($i in hex2c)
			printf("%s ", hex2n[$i])
		else
			printf("--%s-- ", $i)

	print ""
}
