#include <TFile.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TProfile2D.h>
#include <TF1.h>
#include <TStyle.h>
#include <TString.h>
#include <TLatex.h>
#include <TROOT.h>
#include <iostream>

// -------------------------------------------------------------------------
// Macro to merge MALTA_1 Efficiency/Residuals & MALTA_1/MALTA_2 2D Correlations
// -------------------------------------------------------------------------
void check_full_DUT_qc(TString align_filename, TString analysis_filename) {
    gStyle->SetOptStat(0);
    gStyle->SetOptFit(0);
    gStyle->SetPalette(kBird); // High-contrast color palette

    // Open target telemetry file containing all core physics analysis histograms
    TFile *fileAnalysis = TFile::Open(analysis_filename);
    if (!fileAnalysis || fileAnalysis->IsZombie()) {
        std::cout << "[ERROR] Cannot open analysis file: " << analysis_filename << std::endl;
        return;
    }

    std::cout << "[INFO] Loading Target Analysis File: " << analysis_filename << std::endl;

    // Canvas allocation: 3 columns x 2 rows topology grid (6 sub-pads total)
    TCanvas *c1 = new TCanvas("c1", "MALTA2 Performance Tracking Dashboard", 1800, 1100);
    c1->Divide(3, 2);

    // =========================================================================
    // ROW 1: MALTA_1 Core Performance (Efficiency & Adaptive Residuals)
    // =========================================================================

    // --- Pad 1: MALTA_1 In-Pixel Efficiency Map (Square ROI) ---
    c1->cd(1);
    gPad->SetRightMargin(0.15);
    //gPad->SetGrid();
    TString pathEffMap = "AnalysisEfficiency/MALTA_1/inpixelROI/pixelEfficiencyMap_inPixelROI_trackPos_TProfile";
    TProfile2D *hEffMap = (TProfile2D*)fileAnalysis->Get(pathEffMap);
    if (hEffMap) {
        hEffMap->SetTitle("MALTA_1 In-Pixel Efficiency Map (Square ROI)");
        hEffMap->SetDirectory(0);
        hEffMap->GetXaxis()->SetTitle("In-pixel x_{track} [#mum]");
        hEffMap->GetYaxis()->SetTitle("In-pixel y_{track} [#mum]");
        // Symmetrically locked at +/- Pitch/2 to maintain perfect 1:1 aspect ratio
        hEffMap->GetXaxis()->SetRangeUser(-18.2, 18.2);
        hEffMap->GetYaxis()->SetRangeUser(-18.2, 18.2);
        hEffMap->Draw("colz");
    } else {
        std::cout << "[ERROR] Missing object: " << pathEffMap << std::endl;
    }

    // --- Pad 2: MALTA_1 Unbiased Residual X (Sigma-adaptive range) ---
    c1->cd(2);
    TString pathResX = "AnalysisDUT/MALTA_1/local_residuals/residualsX";
    TH1F *hResX = (TH1F*)fileAnalysis->Get(pathResX);
    if (hResX) {
        hResX->SetTitle("MALTA_1 Unbiased Residual X");
        hResX->SetDirectory(0);
        hResX->SetLineColor(kBlue + 2);
        hResX->SetLineWidth(2);
        hResX->GetXaxis()->SetTitle("x_{track} - x_{cluster} [#mum]");

        // Dynamic horizontal range scaling via initial fast fit execution
        hResX->Fit("gaus", "QN"); 
        TF1 *initFitX = hResX->GetFunction("gaus");
        if (initFitX) {
            double sigma = initFitX->GetParameter(2);
            hResX->GetXaxis()->SetRangeUser(-5.0 * sigma, 5.0 * sigma); // Set horizontal range based on sigma
        }

        hResX->Fit("gaus", "Q");
        TF1 *fitX = hResX->GetFunction("gaus");
        hResX->Draw();

        if (fitX) {
            double mean = fitX->GetParameter(1);
            double sigma = fitX->GetParameter(2);
            TLatex latex;
            latex.SetNDC();
            latex.SetTextFont(62);
            latex.SetTextColor(kRed); 
            latex.SetTextSize(0.05);
            latex.DrawLatex(0.15, 0.80, Form("Mean: %.2f #mum", mean));
            latex.DrawLatex(0.15, 0.72, Form("#sigma: %.2f #mum", sigma));
        }
    } else {
        std::cout << "[ERROR] Missing object: " << pathResX << std::endl;
    }

    // --- Pad 3: MALTA_1 Unbiased Residual Y (Sigma-adaptive range) ---
    c1->cd(3);
    TString pathResY = "AnalysisDUT/MALTA_1/local_residuals/residualsY";
    TH1F *hResY = (TH1F*)fileAnalysis->Get(pathResY);
    if (hResY) {
        hResY->SetTitle("MALTA_1 Unbiased Residual Y");
        hResY->SetDirectory(0);
        hResY->SetLineColor(kGreen + 3);
        hResY->SetLineWidth(2);
        hResY->GetXaxis()->SetTitle("y_{track} - y_{cluster} [#mum]");

        hResY->Fit("gaus", "QN");
        TF1 *initFitY = hResY->GetFunction("gaus");
        if (initFitY) {
            double sigma = initFitY->GetParameter(2);
            hResY->GetXaxis()->SetRangeUser(-5.0 * sigma, 5.0 * sigma); // Set horizontal range based on sigma
        }

        hResY->Fit("gaus", "Q");
        TF1 *fitY = hResY->GetFunction("gaus");
        hResY->Draw();

        if (fitY) {
            double mean = fitY->GetParameter(1);
            double sigma = fitY->GetParameter(2);
            TLatex latex;
            latex.SetNDC();
            latex.SetTextFont(62);
            latex.SetTextColor(kRed); 
            latex.SetTextSize(0.05);
            latex.DrawLatex(0.15, 0.80, Form("Mean: %.2f #mum", mean));
            latex.DrawLatex(0.15, 0.72, Form("#sigma: %.2f #mum", sigma));
        }
    } else {
        std::cout << "[ERROR] Missing object: " << pathResY << std::endl;
    }

    // =========================================================================
    // ROW 2: Chip Comparison Space (MALTA_1 vs MALTA_2 2D Local Correlations)
    // =========================================================================

    // --- Pad 4: MALTA_1 2D Local Correlation X ---
    c1->cd(4);
    gPad->SetRightMargin(0.12);
    TString pathCorr1X = "Correlations/MALTA_1/correlationX_2Dlocal";
    TH2F *hCorr1X = (TH2F*)fileAnalysis->Get(pathCorr1X);
    if (hCorr1X) {
        hCorr1X->SetTitle("MALTA_1 2D Local Correlation X");
        hCorr1X->SetDirectory(0);
        hCorr1X->Draw("colz");
    } else {
        std::cout << "[ERROR] Missing object: " << pathCorr1X << std::endl;
    }

    // --- Pad 5: MALTA_2 2D Local Correlation X ---
    c1->cd(5);
    gPad->SetRightMargin(0.12);
    TString pathCorr2X = "Correlations/MALTA_2/correlationX_2Dlocal";
    TH2F *hCorr2X = (TH2F*)fileAnalysis->Get(pathCorr2X);
    if (hCorr2X) {
        hCorr2X->SetTitle("MALTA_2 2D Local Correlation X");
        hCorr2X->SetDirectory(0);
        hCorr2X->Draw("colz");
    } else {
        std::cout << "[ERROR] Missing object: " << pathCorr2X << std::endl;
    }

    // --- Pad 6: MALTA_1 2D Local Correlation Y (Comparison Channel) ---
    c1->cd(6);
    gPad->SetRightMargin(0.12);
    TString pathCorr1Y = "Correlations/MALTA_1/correlationY_2Dlocal";
    TH2F *hCorr1Y = (TH2F*)fileAnalysis->Get(pathCorr1Y);
    if (hCorr1Y) {
        hCorr1Y->SetTitle("MALTA_1 2D Local Correlation Y");
        hCorr1Y->SetDirectory(0);
        hCorr1Y->Draw("colz");
    } else {
        std::cout << "[ERROR] Missing object: " << pathCorr1Y << std::endl;
    }

    c1->Modified();
    c1->Update();
    
    // Serialization
    c1->SaveAs("alignment_qc_result.png");
    c1->SaveAs("alignment_qc_result.pdf");
    
    std::cout << "[DONE] Final Unified Performance Dashboard successfully compiled." << std::endl;
}