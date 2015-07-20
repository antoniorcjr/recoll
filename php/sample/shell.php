<?php
$splitter=chr(7).chr(8).chr(1).chr(2);

$query = recoll_connect();
$rescnt = $query->query("python",120,16);
if( false == $rescnt )
{
    echo "error during query\n";
    var_dump($rescnt);
    return;
}
print "total results ".$rescnt."\n";

for ($i = 0; $i < $rescnt; $i++)
{ 
    $res = $query->get_doc($i);

    if( false == $res)
    {
	echo "error\n";
	return;
    }

    $token=strtok($res,$splitter);
    while ($token != false)
    {
	echo "$token\n";
	$token = strtok($splitter);
    } 
}
?>
