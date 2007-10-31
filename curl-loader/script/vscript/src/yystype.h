#ifndef _YYSTYPE_H_
#define _YYSTYPE_H_


#define  YYSTYPE_IS_DECLARED 

struct tagAST_BASE;

typedef union {
	char   *string_value;
	double  double_value;
	long	long_value;
	int		int_value;
	struct tagAST_BASE *ast;	

} YYSTYPE;



#define YYLTYPE_IS_DECLARED

typedef struct YYLTYPE
{
	int file_id;	// offset of file entry object (what is the file that parsed this one here)
	
	int first_line;
	int first_column;
	int last_line;
	int last_column;

} YYLTYPE;

#define YYLTYPE_set_null(x) \
	do { (x).file_id = (x).first_line =  \
		 (x).last_line = (x).first_column =  \
		 (x).last_column = -1; } while(0);

#endif

