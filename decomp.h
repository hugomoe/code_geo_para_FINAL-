#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "affine.h"
#include "homo_box.h"
#include <omp.h>

//for the homographies
// 0 1 2
// 3 4 5
// 6 7 8

//for the affinities
// 0 1 2
// 3 4 5

//y = H(x)
static void apply_homography(double y[2], double H[3][3], double x[2])
{
	double X = H[0][0] * x[0] + H[0][1] * x[1] + H[0][2];
	double Y = H[1][0] * x[0] + H[1][1] * x[1] + H[1][2];
	double Z = H[2][0] * x[0] + H[2][1] * x[1] + H[2][2];
	y[0] = X / Z;
	y[1] = Y / Z;
}


// H = A H0 B
void decomp(double H[9],double A[6],double H0[9],double B[6]){
/**
  * @param
  *     H an input homography
  *		H0 an output homography
  *		A,B output affinities 
  */
    double a=H[0], b=H[1], p=H[2], c=H[3], d=H[4], q=H[5], r=H[6], s=H[7], t=H[8];
    double t0, t1;
	//assume r,s != 0,0

    //an infinity of (t0,t1) are possible. here is a simple one
    if(fabs(a*s-b*r)<fabs(c*s-d*r)){
        t0 = 0.;
        t1 = -((a*s-b*r)*(a*r+b*s)+(c*s-d*r)*(c*r+d*s))/(pow(r,2.)+pow(s,2.))/(c*s-d*r);
    }else{
        t0 = -((a*s-b*r)*(a*r+b*s)+(c*s-d*r)*(c*r+d*s))/(pow(r,2.)+pow(s,2.))/(a*s-b*r);;
        t1 = 0.;
    }



    //translation of (t0,t1) (H becomes T_(t0,t1)*H)
    a += t0*r;
    b += t0*s;
    p += t0*t;
    c += t1*r;
    d += t1*s;
    q += t1*t;



    double Nphi = sqrt(pow(r,2.)+pow(s,2.));
    B[0] = r/Nphi;
	B[1] = s/Nphi;
	B[2] = 0.;
	B[3] = -s/Nphi;
	B[4] = r/Nphi;
	B[5] = 0.;

    double Npsi = sqrt(pow(a*s-b*r,2.)+pow(c*s-d*r,2.));
	A[0] = (c*s-d*r)/Npsi;
	A[1] = (a*s-b*r)/Npsi;
	A[2] = -t0;
	A[3] = -(a*s-b*r)/Npsi;
	A[4] = (c*s-d*r)/Npsi;
	A[5] = -t1;

    H0[0] = -(a*d-b*c)*Nphi/Npsi;
    H0[1] = 0.;
    H0[2] = (p*(c*s-d*r)-q*(a*s-b*r))/Npsi;
    H0[3] = 0.;
    H0[4] = -Npsi/Nphi;
    H0[5] = (p*(a*s-b*r)+q*(c*s-d*r))/Npsi;
    H0[6] = Nphi;
    H0[7] = 0.;
    H0[8] = t;

}


void apply_homo_final(float *img,float *img_f,int w,int h,int w_f,int h_f,double H[3][3]){

/**
  * @param
  *     img : input image
  *     img_f : output image
  *     w,h : width and heights of the input image
  *		w_f,h_f : width and heights of the output image
  *     H : inverse homohraphy img_f(x,y)=img(H(x,y))
  */


	double *a = *H;

	if(a[6]==0 && a[7]==0){ 	//in this case H is an affinity
        for(int i=0;i<8;i++){a[i]=a[i]/a[8];}
        a[8]=1.;
		double A[6];
		for(int i=0;i<6;i++){A[i]=a[i];}
		apply_affinity(img,img_f,w,h,w_f,h_f,A);
	}
	else{	//here it is an homography
		double A[6];
		double H0[9];
		double B[6];

		decomp(a,A,H0,B);	//compute the decomposition H = A H0 B	



		double x4[4] = {0,w_f,w_f,0};
        double y4[4] = {0,0,h_f,h_f};

        //x1,y1 = H(x4,y4)
        double x1[4];
        double y1[4];

        int w1=w;
        int h1=h;
        int mu1=0;
        int nu1=0;

		x1[0]=0; x1[1]=w; x1[2]=w; x1[3]=0;
		y1[0]=0; y1[1]=0; y1[2]=h; y1[3]=h;



		//x2,y2 = AA(x1,y1) où AA = A^(-1)
		double det_A = A[0]*A[4]-A[3]*A[1];
        double AA[6] = {
            A[4]/det_A,
            -A[1]/det_A,
            (A[1]*A[5]-A[4]*A[2])/det_A,
            -A[3]/det_A,
            A[0]/det_A,
            (-A[0]*A[5]+A[3]*A[2])/det_A
        };

        double x2[4];
        double y2[4];
        for(int k=0;k<4;k++){
            x2[k]=x1[k]*AA[0]+y1[k]*AA[1]+AA[2];
            y2[k]=x1[k]*AA[3]+y1[k]*AA[4]+AA[5];
        }
        double temp_min_x2 = x2[0];
        double temp_min_y2 = y2[0];
        double temp_max_x2 = x2[0];
        double temp_max_y2 = y2[0];
        for(int k=1;k<4;k++){
            if(x2[k]<temp_min_x2){temp_min_x2 = x2[k];}
            if(y2[k]<temp_min_y2){temp_min_y2 = y2[k];}
            if(x2[k]>temp_max_x2){temp_max_x2 = x2[k];}
            if(y2[k]>temp_max_y2){temp_max_y2 = y2[k];}
        }

        int min_x2 = floor(temp_min_x2);
        int min_y2 = floor(temp_min_y2);
        int max_x2 = ceil(temp_max_x2);
        int max_y2 = ceil(temp_max_y2);
        int w2 = max_x2-min_x2, h2 = max_y2-min_y2, mu2 = min_x2, nu2 = min_y2;

        //increase w2, h2 to have the same parity as w1, h1
        if((w2-w1)%2!=0){w2++;}
        if((h2-h1)%2!=0){h2++;}


        //x3,y3 = B(x4,y4)
        double x3[4];
        double y3[4];
        for(int k=0;k<4;k++){
            x3[k]=x4[k]*B[0]+y4[k]*B[1]+B[2];
            y3[k]=x4[k]*B[3]+y4[k]*B[4]+B[5];
        }
        double temp_min_x3 = x3[0];
        double temp_min_y3 = y3[0];
        double temp_max_x3 = x3[0];
        double temp_max_y3 = y3[0];
        for(int k=1;k<4;k++){
            if(x3[k]<temp_min_x3){temp_min_x3 = x3[k];}
            if(y3[k]<temp_min_y3){temp_min_y3 = y3[k];}
            if(x3[k]>temp_max_x3){temp_max_x3 = x3[k];}
            if(y3[k]>temp_max_y3){temp_max_y3 = y3[k];}
        }
        int min_x3 = floor(temp_min_x3);
        int min_y3 = floor(temp_min_y3);
        int max_x3 = ceil(temp_max_x3);
        int max_y3 = ceil(temp_max_y3);
        int w3 = max_x3-min_x3, h3 = max_y3-min_y3, mu3 = min_x3, nu3 = min_y3;
 
 		//increase w3, h3 to have the same parity as w_f, h_f
        if((w3-w_f)%2!=0){w3++;}
        if((h3-h_f)%2!=0){h3++;}



        int w4 = w_f, h4 = h_f;
        int mu4 = 0, nu4 = 0;


        //à ce stade, on a pris en compte une grande partie des translations de A et , via les mu,nu
        //on modifie donc A et B en conséquence

        A[2] = mu2*A[0] + nu2*A[1] + A[2] - mu1;
        A[5] = mu2*A[3] + nu2*A[4] + A[5] - nu1;
        B[2] = mu4*B[0] + nu4*B[1] + B[2] - mu3;
        B[5] = mu4*B[3] + nu4*B[4] + B[5] - nu3;



        ///Application of the decomposition
        
        float *img1 = malloc(3*w1*h1*sizeof(float));
		for(int i=0;i<3*w1*h1;i++){img1[i]=img[i];}

        float *img2 = malloc(3*w2*h2*sizeof(float));
		apply_affinite(img1,img2,w1,h1,w2,h2,A);
		free(img1);

		float *img3 = malloc(3*w3*h3*sizeof(float));
		apply_homo(img2,img3,w2,h2,w3,h3,mu2,nu2,mu3,nu3,H0);
		free(img2);

		float *img4 = malloc(3*w_f*h_f*sizeof(float));
		apply_affinite(img3,img4,w3,h3,w4,h4,B);
		free(img3);

        for(int i=0;i<w_f;i++)
            for(int j=0;j<h_f;j++)
                for(int l=0;l<3;l++)
                    {img_f[(i+w_f*j)*3+l]=img4[(i+w4*j)*3+l];}
        free(img4);
	}
	
	
	double p[2];
	
	//truncate the output image, because it has been symmetrized to prevent ringing
	for(int i=0;i<w_f;i++){
		for(int j=0;j<h_f;j++){
			p[0]=i; p[1]=j;
			apply_homography(p,H,p);
			p[0] = (p[0] - 0.5) * w / (w - 1.0);
			p[1] = (p[1] - 0.5) * h / (h - 1.0);
			if(p[0]<0 || p[0]>w || p[1]<0 || p[1]>h ){
				for(int l=0;l<3;l++){img_f[(j*w_f+i)*3+l]=0;}
			}
		}
	}
	
}





